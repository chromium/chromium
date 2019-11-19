// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_STUB_AUTHENTICATOR_BUILDER_H_
#define CHROMEOS_LOGIN_AUTH_STUB_AUTHENTICATOR_BUILDER_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/stub_authenticator.h"
#include "chromeos/login/auth/user_context.h"

namespace chromeos {

// Helper class for creating a StubAuthenticator with certain configuration.
// Useful in tests for injecting StubAuthenticators to be used during user
// login.
class COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH) StubAuthenticatorBuilder {
 public:
  explicit StubAuthenticatorBuilder(const UserContext& expected_user_context);
  ~StubAuthenticatorBuilder();

  scoped_refptr<Authenticator> Create(AuthStatusConsumer* consumer);

  // Sets up the stub Authenticator to report password changed.
  // |old_password| - the expected old user password. The authenticator will use
  //   it to handle data migration requests (it will report failure if the
  //   provided old password does not match this one).
  // |notifier| - can be empty. If set called when a user data recovery is
  //     attempted.
  void SetUpPasswordChange(
      const std::string& old_password,
      const StubAuthenticator::DataRecoveryNotifier& notifier);

  // Sets up the stub Authenticator to report that user's cryptohome was
  // encrypted using old encryption method, and should be migrated accordingly.
  // |has_incomplete_migration| - whether a migration was attempted but did not
  //     complete.
  void SetUpOldEncryption(bool has_incomplete_migration);

  // Sets up the stub Authenticator to report an auth failure.
  // |failure_reason| - the failure reason to be reported
  void SetUpAuthFailure(AuthFailure::FailureReason failure_reason);

 private:
  const UserContext expected_user_context_;

  // Action to be performed on successful auth against expected user context..
  StubAuthenticator::AuthAction auth_action_ =
      StubAuthenticator::AuthAction::kAuthSuccess;

  // For kPasswordChange action, the old user password.
  std::string old_password_;
  // For kPasswordChange action, the callback to be called to report user data
  // recovery result.
  StubAuthenticator::DataRecoveryNotifier data_recovery_notifier_;

  // For kOldEncryption action - whether an incomplete migration
  // attempt exists.
  bool has_incomplete_encryption_migration_ = false;

  // For kAuthFailure action - the failure reason.
  AuthFailure::FailureReason failure_reason_ = AuthFailure::NONE;

  DISALLOW_COPY_AND_ASSIGN(StubAuthenticatorBuilder);
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_STUB_AUTHENTICATOR_BUILDER_H_
