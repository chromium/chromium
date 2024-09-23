// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/http_auth_manager_impl.h"

#include <utility>

#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"

namespace password_manager {

namespace {
using Logger = autofill::SavePasswordProgressLogger;
}  // anonymous namespace

HttpAuthManagerImpl::HttpAuthManagerImpl(PasswordManagerClient* client)
    : client_(client), observer_(nullptr) {
  DCHECK(client_);
}

HttpAuthManagerImpl::HttpAuthManagerImpl(PasswordManagerClient* client,
                                         HttpAuthObserver* observer,
                                         const PasswordForm& observed_form)
    : HttpAuthManagerImpl(client) {
  SetObserverAndDeliverCredentials(observer, observed_form);
}

HttpAuthManagerImpl::~HttpAuthManagerImpl() {
  if (observer_) {
    observer_->OnLoginModelDestroying();
  }
}

void HttpAuthManagerImpl::DetachObserver(HttpAuthObserver* observer) {
  // Only detach |observer| if it has the same memory address as the stored
  // |observer_|.
  if (observer == observer_) {
    LogMessage(Logger::STRING_HTTPAUTH_ON_DETACH_OBSERVER);
    observer_ = nullptr;
  }
}

void HttpAuthManagerImpl::SetObserverAndDeliverCredentials(
    HttpAuthObserver* observer,
    const PasswordForm& observed_form) {
  LogMessage(Logger::STRING_HTTPAUTH_ON_SET_OBSERVER);
  // Set the observer and communicate the signon_realm.
  // If a previous view registered itself as an observer, it must be notified
  // about its replacement.
  if (observer_) {
    observer_->OnLoginModelDestroying();
  }
  observer_ = observer;

  if (!client_->IsFillingEnabled(observed_form.url)) {
    return;
  }
  // Initialize the form manager.
  form_manager_ = std::make_unique<PasswordFormManager>(
      client_, PasswordFormDigest(observed_form), nullptr /* form_fetcher */,
      std::make_unique<PasswordSaveManagerImpl>(client_));
}

void HttpAuthManagerImpl::ProvisionallySaveForm(
    const PasswordForm& password_form) {
  if (form_manager_) {
    form_manager_->ProvisionallySaveHttpAuthForm(password_form);
  }
}

void HttpAuthManagerImpl::Autofill(
    const PasswordForm& preferred_match,
    const PasswordFormManagerForUI* form_manager) const {
  DCHECK_NE(PasswordForm::Scheme::kHtml, preferred_match.scheme);
  if (observer_ && (form_manager_.get() == form_manager) &&
      client_->IsFillingEnabled(form_manager_->GetURL())) {
    observer_->OnAutofillDataAvailable(preferred_match.username_value,
                                       preferred_match.password_value);
  }
}

void HttpAuthManagerImpl::OnPasswordFormSubmitted(
    const PasswordForm& password_form) {
  if (client_->IsSavingAndFillingEnabled(password_form.url) &&
      !password_form.password_value.empty()) {
    ProvisionallySaveForm(password_form);
  }
}

void HttpAuthManagerImpl::OnPasswordFormDismissed() {
  form_dismissed_ = true;
}

void HttpAuthManagerImpl::OnDidFinishMainFrameNavigation() {
  // Only pay attention to the navigation if the password form (a native browser
  // UI for HTTP auth) has been dismissed. This is necessary because of
  // committed interstitials (https://crbug.com/963307), when the server sends
  // an empty response body with a 401/407 response. In this case, the renderer
  // synthesizes contents for the response and renders it underneath the auth
  // prompt, which looks like a second navigation commit from the browser
  // process's perspective (https://crbug.com/943610). Clearing |form_manager_|
  // on this second commit would be premature and break password saving, so
  // defer it until the password form is actually dismissed. If error pages are
  // changed to no longer double-commit, we can remove the |form_dismissed_|
  // logic.
  if (!form_dismissed_) {
    return;
  }

  form_dismissed_ = false;

  // The login was successful if and only if there were no HTTP errors.
  if (!client_->WasLastNavigationHTTPError()) {
    OnLoginSuccesfull();
  }
  form_manager_.reset();
}

void HttpAuthManagerImpl::OnLoginSuccesfull() {
  LogMessage(Logger::STRING_HTTPAUTH_ON_ASK_USER_OR_SAVE_PASSWORD);
  if (!form_manager_ ||
      !client_->IsSavingAndFillingEnabled(form_manager_->GetURL())) {
    return;
  }

  // ProvisionallySaveForm() might not have called, so |form_manager_| might be
  // not in submitted state. Do nothing in that case.
  if (!form_manager_->is_submitted()) {
    return;
  }

  if (form_manager_->GetFormFetcher()->GetState() ==
      FormFetcher::State::WAITING) {
    // We have a provisional save manager, but it didn't finish matching yet.
    // We just give up.
    return;
  }

  // TODO(crbug.com/40570965) Move the logic into the PasswordFormManager.
  bool is_update = form_manager_->IsPasswordUpdate();
  bool is_new_login = form_manager_->IsNewLogin();
  if (is_update || is_new_login) {
    client_->PromptUserToSaveOrUpdatePassword(std::move(form_manager_),
                                              is_update);
    LogMessage(Logger::STRING_HTTPAUTH_ON_PROMPT_USER);
  } else {
    // For existing credentials that haven't been updated invoke
    // form_manager_->Save() in order to update meta data fields (e.g. last used
    // timestamp).
    form_manager_->Save();
  }
}

void HttpAuthManagerImpl::LogMessage(const Logger::StringID msg) const {
  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    logger.LogMessage(msg);
  }
}
}  // namespace password_manager
