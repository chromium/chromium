// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_AUTH_MANAGER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_AUTH_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/password_manager/core/browser/http_auth_observer.h"

namespace password_manager {

class PasswordManagerClient;
class PasswordFormManager;
class PasswordFormManagerForUI;

// Implementation of the HttpAuthManager as used by the PasswordManagerClient.
class HttpAuthManagerImpl : public HttpAuthManager {
 public:
  explicit HttpAuthManagerImpl(PasswordManagerClient* client);

  HttpAuthManagerImpl(PasswordManagerClient* client,
                      HttpAuthObserver* observer,
                      const autofill::PasswordForm& observed_form);

  ~HttpAuthManagerImpl() override;

  // HttpAuthManager:
  void SetObserverAndDeliverCredentials(
      HttpAuthObserver* observer,
      const autofill::PasswordForm& observed_form) override;
  void DetachObserver(HttpAuthObserver* observer) override;
  void OnPasswordFormSubmitted(
      const autofill::PasswordForm& password_form) override;
  void OnPasswordFormDismissed() override;

  // Called by a PasswordManagerClient when it decides that a HTTP auth dialog
  // can be auto-filled. It notifies the observer about new credentials given
  // that the form manged by |form_manager| equals the one observed by the
  // observer that is managed by |form_manager|.
  void Autofill(const autofill::PasswordForm& preferred_match,
                const PasswordFormManagerForUI* form_manager) const;

  // Handles successful navigation to the main frame.
  void OnDidFinishMainFrameNavigation();

 private:
  // Get a Logger object and write a log message defined by the message id.
  void LogMessage(const BrowserSavePasswordProgressLogger::StringID) const;

  // Passes |form| to PasswordFormManager that manages it for using it after
  // detecting submission success for saving.
  void ProvisionallySaveForm(const autofill::PasswordForm& password_form);

  // Initiates the saving of the password.
  void OnLoginSuccesfull();

  // The embedder-level client. Must outlive this class.
  PasswordManagerClient* const client_;

  // Observer to be notified about values to be filled in.
  HttpAuthObserver* observer_;

  // Single password form manager to handle the http-auth request form.
  std::unique_ptr<PasswordFormManager> form_manager_;

  // When set to true, the password form has been dismissed and |form_manager_|
  // will be cleared on next navigation.
  bool form_dismissed_ = false;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_AUTH_MANAGER_IMPL_H_
