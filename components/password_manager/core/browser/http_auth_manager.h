// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_AUTH_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_AUTH_MANAGER_H_

namespace password_manager {

class PasswordManagerClient;
class HttpAuthObserver;
struct PasswordForm;

// Per-tab password manager for http-auth forms.
// This class is the counterpart to the PasswordManager which manages storing
// credentials of html login forms.

// Defines the public interface used outside of the PasswordManagerClient.
class HttpAuthManager {
 public:
  HttpAuthManager() = default;
  virtual ~HttpAuthManager() = default;

  // Set the observer which is notified in case a form can be auto-filled.
  virtual void SetObserverAndDeliverCredentials(
      HttpAuthObserver* observer,
      const PasswordForm& observed_form) = 0;

  // Detach |observer| as the observer if it is the current observer.
  // Called by the observer when destructed to unregister itself.
  virtual void DetachObserver(HttpAuthObserver* observer) = 0;

  // Handles submitted http-auth credentials event.
  // Called by the LoginHandler instance.
  virtual void OnPasswordFormSubmitted(const PasswordForm& password_form) = 0;

  // Called by the LoginHandler instance when the password form is dismissed.
  virtual void OnPasswordFormDismissed() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_AUTH_MANAGER_H_
