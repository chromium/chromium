// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/remote_actor_selection_dialog_controller.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

constexpr char kRemoteActorDataHandlingHelpUrl[] = "https://support.google.com";

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

PasswordCombinedSelectorController::DisplayType
RemoteActorSelectionDialogController::GetDisplayType() const {
  return DisplayType::kRemoteActor;
}

bool RemoteActorSelectionDialogController::ShouldShowTopIllustration() const {
  return true;
}

std::u16string RemoteActorSelectionDialogController::GetTitle() const {
  // TODO(crbug.com/558163687): Enable translations closer to launch.
  std::u16string formatted_domain = url_formatter::FormatUrlForSecurityDisplay(
      GURL(credential_domain_),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  return l10n_util::GetStringFUTF16(IDS_REMOTE_ACTOR_SHARING_DIALOG_TITLE,
                                    formatted_domain);
}

std::u16string RemoteActorSelectionDialogController::GetSubtitle() const {
  // TODO(crbug.com/558163687): Enable translations closer to launch.
  std::vector<size_t> offsets;
  std::u16string subtitle = l10n_util::GetStringFUTF16(
      IDS_REMOTE_ACTOR_SHARING_DIALOG_SUBTITLE,
      {std::u16string(), std::u16string()}, &offsets);
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
      GURL(kRemoteActorDataHandlingHelpUrl), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      /*is_renderer_initiated=*/false);
  web_contents_->OpenURL(params, /*navigation_handle_callback=*/{});
}

std::u16string RemoteActorSelectionDialogController::GetOkButtonLabel() const {
  // TODO(crbug.com/558163687): Enable translations closer to launch.
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
  // TODO(crbug.com/532483845): Upload selected credential to Passbox and grant
  // permission in APS.
  // TODO(crbug.com/532482931): Notify Private JS API of success.
  DestroyDialog();
  if (callback_) {
    std::move(callback_).Run(password_form);
  }
}

void RemoteActorSelectionDialogController::OnCloseDialog() {
  // TODO(crbug.com/532482931): Notify Private JS API of cancellation.
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
