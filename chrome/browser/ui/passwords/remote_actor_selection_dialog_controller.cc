// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/remote_actor_selection_dialog_controller.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace password_manager {

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
  // TODO(crbug.com/535945530): Use localized string.
  return u"Allow Gemini Spark to sign in to " +
         url_formatter::FormatUrlForSecurityDisplay(
             GURL(credential_domain_),
             url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC) +
         u" for you?";
}

std::u16string RemoteActorSelectionDialogController::GetSubtitle() const {
  // TODO(crbug.com/535945530): Use localized string.
  // TODO(crbug.com/535854168): Make "Learn how Spark handles your data" a link.
  return u"Spark can use Google Password Manager to sign in for you. Learn how "
         u"Spark handles your data";
}

std::u16string RemoteActorSelectionDialogController::GetOkButtonLabel() const {
  // TODO(crbug.com/535945530): Use localized string.
  return u"Allow this time";
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
