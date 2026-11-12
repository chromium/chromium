// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/ambient/ambient_signin_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_controller.h"
#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_request.h"
#include "chrome/browser/ui/webauthn/webauthn_ui_helpers.h"
#include "chrome/browser/webauthn/webauthn_metrics_util.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::RenderFrameHost;

namespace ambient_signin {

namespace {

constexpr int kMaxDismissalsBeforeCooldown = 2;
constexpr base::TimeDelta kCooldownDuration = base::Days(7);
constexpr base::TimeDelta kPromptDataExpiration = base::Days(30);
constexpr char kDismissalCountKey[] = "count";
constexpr char kLastDismissedKey[] = "last_dismissed";

GURL GetTopLevelUrl(RenderFrameHost& rfh) {
  return rfh.GetOutermostMainFrame()->GetLastCommittedOrigin().GetURL();
}

base::DictValue GetPromptData(const GURL& top_level_url,
                              content::BrowserContext* browser_context) {
  if (!top_level_url.is_valid()) {
    return base::DictValue();
  }
  base::Value stored_value =
      HostContentSettingsMapFactory::GetForProfile(browser_context)
          ->GetWebsiteSetting(
              top_level_url, top_level_url,
              ContentSettingsType::WEBAUTHN_AMBIENT_SIGNIN_DATA);
  return stored_value.is_dict() ? std::move(stored_value).TakeDict()
                                : base::DictValue();
}

void SetPromptData(
    const GURL& top_level_url,
    content::BrowserContext* browser_context,
    base::Value value,
    content_settings::ContentSettingConstraints constraints = {}) {
  if (!top_level_url.is_valid()) {
    return;
  }
  HostContentSettingsMapFactory::GetForProfile(browser_context)
      ->SetWebsiteSettingDefaultScope(
          top_level_url, top_level_url,
          ContentSettingsType::WEBAUTHN_AMBIENT_SIGNIN_DATA, std::move(value),
          constraints);
}

}  // namespace

AmbientSigninController::~AmbientSigninController() {
  if (model_) {
    model_->observers.RemoveObserver(this);
    model_ = nullptr;
    Close();
  }
}

void AmbientSigninController::Show(AuthenticatorRequestDialogModel* model) {
  const bool record_metrics = !std::exchange(is_restarting_, false);
  const ukm::SourceId ukm_source_id = render_frame_host().GetPageUkmSourceId();

  if (!model_) {
    model_ = model;
    model_->observers.AddObserver(this);
  } else {
    CHECK(model == model_);
  }

  credential_indices_.clear();
  for (size_t i = 0; i < model_->mechanisms.size(); ++i) {
    const auto& type = model_->mechanisms[i].type;
    if (std::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Credential>(type) ||
        std::holds_alternative<
            AuthenticatorRequestDialogModel::Mechanism::Password>(type)) {
      credential_indices_.push_back(i);
    }
  }

  if (credential_indices_.empty()) {
    if (record_metrics) {
      webauthn::metrics::RecordAmbientDisplayOutcome(
          ukm_source_id,
          webauthn::metrics::AmbientDisplayOutcome::kNoCredentials);
    }
    return;
  }

  if (!render_frame_host().GetPage().IsPrimary()) {
    if (record_metrics) {
      webauthn::metrics::RecordAmbientDisplayOutcome(
          ukm_source_id,
          webauthn::metrics::AmbientDisplayOutcome::kNotPrimaryPage);
    }
    return;
  }

  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents ||
      !permissions::PermissionRequestManager::FromWebContents(web_contents)) {
    if (record_metrics) {
      webauthn::metrics::RecordAmbientDisplayOutcome(
          ukm_source_id, webauthn::metrics::AmbientDisplayOutcome::
                             kNoPermissionRequestManager);
    }
    return;
  }

  auto* permission_controller =
      AmbientLoginPermissionController::GetOrCreateForPage(
          render_frame_host().GetPage());
  if (permission_controller->state() !=
      AmbientLoginPermissionController::State::kIdle) {
    if (record_metrics) {
      webauthn::metrics::RecordAmbientDisplayOutcome(
          ukm_source_id,
          webauthn::metrics::AmbientDisplayOutcome::kAnotherRequestPending);
    }
    return;
  }

  if (record_metrics) {
    webauthn::metrics::RecordAmbientDisplayOutcome(
        ukm_source_id, webauthn::metrics::AmbientDisplayOutcome::kShown);
  }

  std::vector<ambient_signin::PasskeyOrPasswordCredential> credentials;
  credentials.reserve(credential_indices_.size());
  for (size_t index : credential_indices_) {
    const auto& mechanism = model_->mechanisms.at(index);
    const auto* passkey =
        std::get_if<AuthenticatorRequestDialogModel::Mechanism::Credential>(
            &mechanism.type);
    credentials.push_back({
        /*username=*/mechanism.name,
        /*provider_name=*/mechanism.description,
        /*type=*/
        passkey ? ambient_signin::CredentialType::kPasskey
                : ambient_signin::CredentialType::kPassword,
        /*display_name=*/mechanism.display_name,
        /*provider_icon=*/
        passkey ? webauthn_ui_helpers::GetProviderIcon(passkey->value())
                : &GooglePasswordManagerVectorIcon(),
    });
  }

  const GURL top_level_url = GetTopLevelUrl(render_frame_host());
  const base::DictValue site_dict =
      GetPromptData(top_level_url, render_frame_host().GetBrowserContext());
  const int dismissal_count = site_dict.FindInt(kDismissalCountKey).value_or(0);
  const std::optional<base::Time> last_dismissed =
      base::ValueToTime(site_dict.Find(kLastDismissedKey));
  const bool in_cooldown =
      dismissal_count >= kMaxDismissalsBeforeCooldown &&
      last_dismissed.has_value() &&
      base::Time::Now() - *last_dismissed < kCooldownDuration;
  const bool bubble_starts_open = !model_->ambient_chip_only && !in_cooldown;

  if (record_metrics) {
    if (bubble_starts_open) {
      webauthn::metrics::RecordAmbientInitialUiMode(
          ukm_source_id, webauthn::metrics::AmbientInitialUiMode::kBubble);
    } else if (model_->ambient_chip_only) {
      webauthn::metrics::RecordAmbientInitialUiMode(
          ukm_source_id,
          webauthn::metrics::AmbientInitialUiMode::kChipOnlyRequestedBySite);
    } else {
      webauthn::metrics::RecordAmbientInitialUiMode(
          ukm_source_id,
          webauthn::metrics::AmbientInitialUiMode::kChipOnlyCooldownActive);
    }
  }

  ukm_source_id_ = ukm_source_id;
  request_active_ = record_metrics;
  request_started_as_bubble_ = bubble_starts_open;
  bubble_shown_ = false;
  bubble_dismissed_ = false;
  credential_selected_ = false;
  selected_from_expanded_list_ = false;

  auto request = std::make_unique<AmbientLoginPermissionRequest>(
      render_frame_host().GetLastCommittedOrigin().GetURL(),
      std::move(credentials),
      base::BindOnce(&AmbientSigninController::OnMechanismSelected,
                     GetWeakPtr()),
      AmbientLoginPermissionRequest::Options{
          .permission_decided_callback = base::BindRepeating(
              &AmbientSigninController::OnPermissionDecided, GetWeakPtr()),
          .bubble_starts_open = bubble_starts_open,
          .bubble_shown_callback = base::BindOnce(
              &AmbientSigninController::OnBubbleShown, GetWeakPtr()),
          .bubble_dismissed_callback = base::BindOnce(
              &AmbientSigninController::OnBubbleDismissed, GetWeakPtr())});
  permission_controller->RequestPermission(&render_frame_host(),
                                           std::move(request));
}

AmbientSigninController::AmbientSigninController(
    RenderFrameHost* render_frame_host)
    : content::DocumentUserData<AmbientSigninController>(render_frame_host) {}

void AmbientSigninController::OnMechanismSelected(size_t index,
                                                  bool from_expanded_list) {
  CHECK(index < credential_indices_.size());
  if (!bubble_shown_) {
    OnBubbleShown();
  }
  if (request_active_) {
    webauthn::metrics::RecordAmbientCredentialSelectionMethod(
        from_expanded_list
            ? (index == 0
                   ? webauthn::metrics::AmbientCredentialSelectionMethod::
                         kExpandedDefaultCredentialClicked
                   : webauthn::metrics::AmbientCredentialSelectionMethod::
                         kExpandedNonDefaultCredentialClicked)
            : webauthn::metrics::AmbientCredentialSelectionMethod::
                  kSignInButtonClicked);
  }
  credential_selected_ = true;
  selected_from_expanded_list_ = from_expanded_list;
  FinalizeRequestMetrics();
  // Reset any dismissal count and cooldown for the site.
  SetPromptData(GetTopLevelUrl(render_frame_host()),
                render_frame_host().GetBrowserContext(), base::Value());
  if (model_) {
    model_->mechanisms.at(credential_indices_.at(index)).callback.Run();
  }
}

void AmbientSigninController::OnBubbleShown() {
  if (!request_active_ || bubble_shown_) {
    return;
  }
  bubble_shown_ = true;
  webauthn::metrics::RecordAmbientBubbleCredentialCount(
      credential_indices_.size());
}

void AmbientSigninController::OnBubbleDismissed() {
  if (!bubble_shown_) {
    OnBubbleShown();
  }
  bubble_dismissed_ = true;
  const GURL top_level_url = GetTopLevelUrl(render_frame_host());
  content::BrowserContext* browser_context =
      render_frame_host().GetBrowserContext();
  base::DictValue site_dict = GetPromptData(top_level_url, browser_context);
  const int current_count = site_dict.FindInt(kDismissalCountKey).value_or(0);
  site_dict.Set(kDismissalCountKey, current_count + 1);
  site_dict.Set(kLastDismissedKey, base::TimeToValue(base::Time::Now()));
  content_settings::ContentSettingConstraints constraints;
  constraints.set_lifetime(kPromptDataExpiration);
  SetPromptData(top_level_url, browser_context,
                base::Value(std::move(site_dict)), std::move(constraints));
}

void AmbientSigninController::OnPermissionDecided(
    const permissions::PermissionPromptDecision& decision,
    const permissions::PermissionRequestData& request_data) {
  FinalizeRequestMetrics();
}

void AmbientSigninController::FinalizeRequestMetrics() {
  if (!request_active_) {
    return;
  }
  request_active_ = false;

  if (!request_started_as_bubble_) {
    webauthn::metrics::RecordAmbientChipOnlyOutcome(
        ukm_source_id_,
        bubble_shown_
            ? webauthn::metrics::AmbientChipOnlyOutcome::kBubbleActivated
            : webauthn::metrics::AmbientChipOnlyOutcome::kIgnored);
  }

  if (bubble_shown_) {
    webauthn::metrics::AmbientBubbleResolution resolution;
    webauthn::metrics::AmbientUkmBubbleOutcome ukm_bubble_outcome;
    if (credential_selected_) {
      resolution =
          webauthn::metrics::AmbientBubbleResolution::kCredentialSelected;
      ukm_bubble_outcome = selected_from_expanded_list_
                               ? webauthn::metrics::AmbientUkmBubbleOutcome::
                                     kSelectedInExpandedBubble
                               : webauthn::metrics::AmbientUkmBubbleOutcome::
                                     kSelectedDefaultInUnexpandedBubble;
    } else if (bubble_dismissed_) {
      resolution = webauthn::metrics::AmbientBubbleResolution::kDismissed;
      ukm_bubble_outcome =
          webauthn::metrics::AmbientUkmBubbleOutcome::kDismissed;
    } else {
      resolution = webauthn::metrics::AmbientBubbleResolution::kIgnored;
      ukm_bubble_outcome = webauthn::metrics::AmbientUkmBubbleOutcome::
          kDestroyedWithoutInteraction;
    }
    webauthn::metrics::RecordAmbientBubbleResolution(request_started_as_bubble_,
                                                     resolution);
    webauthn::metrics::RecordAmbientUkmBubbleOutcome(ukm_source_id_,
                                                     ukm_bubble_outcome);
  }
}

void AmbientSigninController::Close() {
  FinalizeRequestMetrics();
  if (auto* permission_controller =
          AmbientLoginPermissionController::GetForPage(
              render_frame_host().GetPage())) {
    // TODO(crbug.com/532206357): Tell the permission controller that
    // passkey/password options are no longer available. If it is showing FedCM
    // it can remain open.
    permission_controller->Reset();
  }
}

void AmbientSigninController::OnStartOver() {
  is_restarting_ = true;
}

void AmbientSigninController::OnRequestComplete() {
  is_restarting_ = false;
  if (model_) {
    model_->observers.RemoveObserver(this);
    model_ = nullptr;
  }
  Close();
}

void AmbientSigninController::OnModelDestroyed(
    AuthenticatorRequestDialogModel* model) {
  CHECK(model == model_);
  is_restarting_ = false;
  model_->observers.RemoveObserver(this);
  model_ = nullptr;
  Close();
}

base::WeakPtr<AmbientSigninController> AmbientSigninController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

DOCUMENT_USER_DATA_KEY_IMPL(AmbientSigninController);

}  // namespace ambient_signin
