// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_STATUS_CONSUMER_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_STATUS_CONSUMER_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"

class AccountId;

namespace ash {

class AuthFailure;
class UserContext;

// An interface that defines the callbacks for objects that the
// Authenticator class will call to report the success/failure of
// authentication for Chromium OS.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH) AuthStatusConsumer
    : public base::CheckedObserver {
 public:
  ~AuthStatusConsumer() override = default;
  // The current login attempt has ended in failure, with error |error|.
  virtual void OnAuthFailure(const AuthFailure& error) = 0;

  // The current login attempt has succeeded for |user_context|.
  virtual void OnAuthSuccess(const UserContext& user_context) = 0;
  // The current guest login attempt has succeeded.
  virtual void OnOffTheRecordAuthSuccess() {}
  // Password verified by the online flow does work for local
  // authentication.
  // This is the method that should actually handle the scenario.
  // `context` has online-verified password as a `Key`, and also as
  // the `OnlinePassword`.
  // `online_password_mismatch` allows to differentiate situation where
  // `OnlinePassword` is present, but fails cryptohome authentication.
  virtual void OnOnlinePasswordUnusable(
      std::unique_ptr<UserContext> user_context,
      bool online_password_mismatch);
  // Auxiliary method, used to get notified about password change without
  // actually handling it.
  virtual void OnPasswordChangeDetectedFor(const AccountId& account);

  // User have successfully went through online authentication, but
  // user has only local knowledge factor, so extra "local authentication"
  // step is required.
  virtual void OnLocalAuthenticationRequired(
      std::unique_ptr<UserContext> user_context);

  // The cryptohome is encrypted in old format and needs migration.
  virtual void OnOldEncryptionDetected(std::unique_ptr<UserContext> context,
                                       bool has_incomplete_migration);
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_AUTH_STATUS_CONSUMER_H_
