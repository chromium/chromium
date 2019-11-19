// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_STUB_AUTHENTICATOR_H_
#define CHROMEOS_LOGIN_AUTH_STUB_AUTHENTICATOR_H_

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/authenticator.h"
#include "chromeos/login/auth/user_context.h"

class AccountId;

namespace content {
class BrowserContext;
}

namespace chromeos {

class AuthStatusConsumer;
class StubAuthenticatorBuilder;

class COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH) StubAuthenticator
    : public Authenticator {
 public:
  enum class DataRecoveryStatus {
    kNone,
    kRecovered,
    kRecoveryFailed,
    kResynced
  };
  using DataRecoveryNotifier =
      base::RepeatingCallback<void(DataRecoveryStatus status)>;

  StubAuthenticator(AuthStatusConsumer* consumer,
                    const UserContext& expected_user_context);

  // Authenticator:
  void CompleteLogin(content::BrowserContext* context,
                     const UserContext& user_context) override;
  void AuthenticateToLogin(content::BrowserContext* context,
                           const UserContext& user_context) override;
  void AuthenticateToUnlock(const UserContext& user_context) override;
  void LoginAsSupervisedUser(const UserContext& user_context) override;
  void LoginOffTheRecord() override;
  void LoginAsPublicSession(const UserContext& user_context) override;
  void LoginAsKioskAccount(const AccountId& app_account_id,
                           bool use_guest_mount) override;
  void LoginAsArcKioskAccount(const AccountId& app_account_id) override;
  void LoginAsWebKioskAccount(const AccountId& app_account_id) override;
  void OnAuthSuccess() override;
  void OnAuthFailure(const AuthFailure& failure) override;
  void RecoverEncryptedData(const std::string& old_password) override;
  void ResyncEncryptedData() override;

  void SetExpectedCredentials(const UserContext& user_context);

 protected:
  ~StubAuthenticator() override;

 private:
  friend class StubAuthenticatorBuilder;

  enum class AuthAction {
    kAuthSuccess,
    kAuthFailure,
    kPasswordChange,
    kOldEncryption
  };

  // Returns a copy of expected_user_context_ with a transformed key.
  UserContext ExpectedUserContextWithTransformedKey() const;

  void OnPasswordChangeDetected();
  void OnOldEncryptionDetected();

  UserContext expected_user_context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // The action taken for login requests that match the expected context.
  AuthAction auth_action_ = AuthAction::kAuthSuccess;

  // For password change requests - the old user password.
  std::string old_password_;

  // If set, the callback that will be called as authenticator handles user
  // encrypted data recovery during password change flow.
  DataRecoveryNotifier data_recovery_notifier_;

  // For requests that detect old encryption -  whether there is an incomplete
  // encryption migration attempt.
  bool has_incomplete_encryption_migration_ = false;

  // For requests that report auth failure, the reason for the failure.
  AuthFailure::FailureReason failure_reason_ = AuthFailure::NONE;

  DISALLOW_COPY_AND_ASSIGN(StubAuthenticator);
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_STUB_AUTHENTICATOR_H_
