// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/remote_actor_selection_dialog_controller.h"

#include <optional>

#include "base/check.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "components/password_manager/core/common/remote_actor/remote_actor_credential_sharing_policy.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace password_manager {

namespace {

constexpr char kRemoteActorDataHandlingHelpUrl[] =
    "https://support.google.com/gemini?p=lm_gpm_data";

std::u16string GetDefaultActorName(RemoteActorType actor_type) {
  switch (actor_type) {
    case RemoteActorType::kGemini:
      return l10n_util::GetStringUTF16(IDS_REMOTE_ACTOR_GEMINI_NAME);
    case RemoteActorType::kSearch:
      return l10n_util::GetStringUTF16(IDS_REMOTE_ACTOR_SEARCH_AGENT_NAME);
  }
  NOTREACHED();
}

}  // namespace

RemoteActorSelectionDialogController::RemoteActorSelectionDialogController(
    content::WebContents* web_contents,
    FormsVector local_credentials,
    const std::string& credential_domain,
    OnResultCallback callback)
    : web_contents_(web_contents),
      local_credentials_(std::move(local_credentials)),
      credential_domain_(credential_domain),
      callback_(std::move(callback)) {}

RemoteActorSelectionDialogController::~RemoteActorSelectionDialogController() {
  DestroyDialog();
  if (callback_) {
    std::move(callback_).Run(std::nullopt);
  }
}

void RemoteActorSelectionDialogController::Show() {
  if (!web_contents_) {
    return;
  }
  view_ = CreatePasswordCombinedSelectorPromptView(this, web_contents_);
  view_->ShowAccountChooser();
}

void RemoteActorSelectionDialogController::SetActorType(
    RemoteActorType actor_type) {
  CHECK(!view_);
  actor_type_ = actor_type;
  subtitle_link_range_ = gfx::Range();
}

void RemoteActorSelectionDialogController::SetActorName(
    std::u16string actor_name) {
  CHECK(!view_);
  actor_name_ = std::move(actor_name);
  subtitle_link_range_ = gfx::Range();
}

void RemoteActorSelectionDialogController::SetLearnMoreUrl(
    GURL learn_more_url) {
  CHECK(!view_);
  learn_more_url_ = std::move(learn_more_url);
}

void RemoteActorSelectionDialogController::SetActorIconUrl(
    GURL actor_icon_url) {
  CHECK(!view_);
  actor_icon_url_ = std::move(actor_icon_url);
}

GURL RemoteActorSelectionDialogController::GetLearnMoreUrl() const {
  if (learn_more_url_.is_valid() &&
      learn_more_url_.SchemeIs(url::kHttpsScheme)) {
    return learn_more_url_;
  }
  return GURL(kRemoteActorDataHandlingHelpUrl);
}

PasswordCombinedSelectorController::DisplayType
RemoteActorSelectionDialogController::GetDisplayType() const {
  return DisplayType::kRemoteActor;
}

bool RemoteActorSelectionDialogController::ShouldShowTopIllustration() const {
  return true;
}

std::optional<RemoteActorType>
RemoteActorSelectionDialogController::GetActorType() const {
  return actor_type_;
}

GURL RemoteActorSelectionDialogController::GetActorIconUrl() const {
  if (actor_icon_url_.is_valid() &&
      actor_icon_url_.SchemeIs(url::kHttpsScheme)) {
    return actor_icon_url_;
  }
  return GURL();
}

std::u16string RemoteActorSelectionDialogController::GetTitle() const {
  std::u16string formatted_domain = url_formatter::FormatUrlForSecurityDisplay(
      GURL(credential_domain_),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  const std::u16string actor_name =
      actor_name_.empty() ? GetDefaultActorName(actor_type_) : actor_name_;
  return l10n_util::GetStringFUTF16(IDS_REMOTE_ACTOR_SHARING_DIALOG_TITLE,
                                    actor_name, formatted_domain);
}

std::u16string RemoteActorSelectionDialogController::GetSubtitle() const {
  const std::u16string actor_name =
      actor_name_.empty() ? GetDefaultActorName(actor_type_) : actor_name_;
  std::vector<size_t> offsets;
  std::u16string subtitle = l10n_util::GetStringFUTF16(
      IDS_REMOTE_ACTOR_SHARING_DIALOG_SUBTITLE,
      {std::u16string(), std::u16string(), actor_name}, &offsets);
  if (offsets.size() >= 2) {
    subtitle_link_range_ = gfx::Range(offsets[0], offsets[1]);
  }
  return subtitle;
}

gfx::Range RemoteActorSelectionDialogController::GetSubtitleLinkRange() const {
  if (subtitle_link_range_.is_empty()) {
    GetSubtitle();
  }
  return subtitle_link_range_;
}

void RemoteActorSelectionDialogController::OnSubtitleLinkClicked() {
  if (!web_contents_) {
    return;
  }
  content::OpenURLParams params(
      GetLearnMoreUrl(), content::Referrer(),
      ui::mojom::WindowOpenDisposition::kNewForegroundTab,
      ui::PAGE_TRANSITION_LINK,
      /*is_renderer_initiated=*/false);
  web_contents_->OpenURL(params, /*navigation_handle_callback=*/{});
}

std::u16string RemoteActorSelectionDialogController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_REMOTE_ACTOR_SHARING_DIALOG_ALLOW_BUTTON);
}

const PasswordCombinedSelectorController::FormsVector&
RemoteActorSelectionDialogController::GetLocalForms() const {
  return local_credentials_;
}

bool RemoteActorSelectionDialogController::IsShowingAccountChooser() const {
  return view_ != nullptr;
}

void RemoteActorSelectionDialogController::OnChooseCredentials(
    const password_manager::PasswordForm& password_form,
    password_manager::CredentialType credential_type) {
  DestroyDialog();
  if (callback_) {
    std::move(callback_).Run(password_form);
  }
}

void RemoteActorSelectionDialogController::OnCloseDialog() {
  DestroyDialog();
  if (callback_) {
    std::move(callback_).Run(std::nullopt);
  }
}

void RemoteActorSelectionDialogController::DestroyDialog() {
  if (view_) {
    view_->ControllerGone();
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(view_));
  }
}

}  // namespace password_manager
