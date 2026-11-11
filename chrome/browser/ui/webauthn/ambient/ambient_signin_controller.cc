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
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/document_user_data.h"
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

  CHECK(!credential_indices_.empty());

  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    return;
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
  const bool bubble_starts_open =
      !model_->ambient_chip_only &&
      (dismissal_count < kMaxDismissalsBeforeCooldown ||
       !last_dismissed.has_value() ||
       base::Time::Now() - *last_dismissed >= kCooldownDuration);

  auto* permission_controller =
      AmbientLoginPermissionController::GetOrCreateForPage(
          render_frame_host().GetPage());
  auto request = std::make_unique<AmbientLoginPermissionRequest>(
      render_frame_host().GetLastCommittedOrigin().GetURL(),
      std::move(credentials),
      base::BindOnce(&AmbientSigninController::OnMechanismSelected,
                     GetWeakPtr()),
      AmbientLoginPermissionRequest::Options{
          .bubble_starts_open = bubble_starts_open,
          .bubble_dismissed_callback = base::BindOnce(
              &AmbientSigninController::OnBubbleDismissed, GetWeakPtr())});
  permission_controller->RequestPermission(&render_frame_host(),
                                           std::move(request));
}

AmbientSigninController::AmbientSigninController(
    RenderFrameHost* render_frame_host)
    : content::DocumentUserData<AmbientSigninController>(render_frame_host) {}

void AmbientSigninController::OnMechanismSelected(size_t index) {
  CHECK(index < credential_indices_.size());
  // Reset any dismissal count and cooldown for the site.
  SetPromptData(GetTopLevelUrl(render_frame_host()),
                render_frame_host().GetBrowserContext(), base::Value());
  if (model_) {
    model_->mechanisms.at(credential_indices_.at(index)).callback.Run();
  }
}

void AmbientSigninController::OnBubbleDismissed() {
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

void AmbientSigninController::Close() {
  if (auto* permission_controller =
          AmbientLoginPermissionController::GetForPage(
              render_frame_host().GetPage())) {
    // TODO(crbug.com/532206357): Tell the permission controller that
    // passkey/password options are no longer available. If it is showing FedCM
    // it can remain open.
    permission_controller->Reset();
  }
}

void AmbientSigninController::OnRequestComplete() {
  if (model_) {
    model_->observers.RemoveObserver(this);
    model_ = nullptr;
  }
  Close();
}

void AmbientSigninController::OnModelDestroyed(
    AuthenticatorRequestDialogModel* model) {
  CHECK(model == model_);
  model_->observers.RemoveObserver(this);
  model_ = nullptr;
  Close();
}

base::WeakPtr<AmbientSigninController> AmbientSigninController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

DOCUMENT_USER_DATA_KEY_IMPL(AmbientSigninController);

}  // namespace ambient_signin
