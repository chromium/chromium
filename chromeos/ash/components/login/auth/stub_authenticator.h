// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_STUB_AUTHENTICATOR_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_STUB_AUTHENTICATOR_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "chromeos/ash/components/login/auth/authenticator.h"
#include "chromeos/ash/components/login/auth/public/auth_failure.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/user_manager/user_type.h"

class AccountId;

namespace ash {

class AuthStatusConsumer;
class StubAuthenticatorBuilder;

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH) StubAuthenticator
    : public Authenticator {
 public:
  StubAuthenticator(AuthStatusConsumer* consumer,
                    const UserContext& expected_user_context);

  StubAuthenticator(const StubAuthenticator&) = delete;
  StubAuthenticator& operator=(const StubAuthenticator&) = delete;

  // Authenticator:
  void CompleteLogin(bool ephemeral,
                     std::unique_ptr<UserContext> user_context) override;
  void AuthenticateToLogin(bool ephemeral,
                           std::unique_ptr<UserContext> user_context) override;
  void LoginOffTheRecord() override;
  void LoginAsPublicSession(const UserContext& user_context) override;
  void AuthenticateToUnlock(bool ephemeral,
                            std::unique_ptr<UserContext> user_context) override;
  void LoginAsKioskAccount(const AccountId& app_account_id,
                           bool ephemeral) override;
  void LoginAsWebKioskAccount(const AccountId& app_account_id,
                              bool ephemeral) override;
  void LoginAsIwaKioskAccount(const AccountId& app_account_id,
                              bool ephemeral) override;
  void LoginAuthenticated(std::unique_ptr<UserContext> user_context) override;
  void OnAuthSuccess() override;
  void OnAuthFailure(const AuthFailure& failure) override;

  void SetExpectedCredentials(const UserContext& user_context);

 protected:
  ~StubAuthenticator() override;

 private:
  friend class StubAuthenticatorBuilder;

  enum class AuthAction { kAuthSuccess, kAuthFailure, kOldEncryption };

  // Returns a copy of expected_user_context_ with a transformed key.
  UserContext ExpectedUserContextWithTransformedKey() const;

  void OnPasswordChangeDetected();
  void OnOldEncryptionDetected();

  void LoginAsKioskAccountStub(user_manager::UserType kiosk_type);

  UserContext expected_user_context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // The action taken for login requests that match the expected context.
  AuthAction auth_action_ = AuthAction::kAuthSuccess;

  // For password change requests - the old user password.
  std::string old_password_;

  // For requests that detect old encryption -  whether there is an incomplete
  // encryption migration attempt.
  bool has_incomplete_encryption_migration_ = false;

  // For requests that report auth failure, the reason for the failure.
  AuthFailure::FailureReason failure_reason_ = AuthFailure::NONE;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_STUB_AUTHENTICATOR_H_
