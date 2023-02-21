// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/stub_authenticator_builder.h"

namespace ash {

StubAuthenticatorBuilder::StubAuthenticatorBuilder(
    const UserContext& expected_user_context)
    : expected_user_context_(expected_user_context) {}

StubAuthenticatorBuilder::~StubAuthenticatorBuilder() = default;

scoped_refptr<Authenticator> StubAuthenticatorBuilder::Create(
    AuthStatusConsumer* consumer) {
  scoped_refptr<StubAuthenticator> authenticator =
      new StubAuthenticator(consumer, expected_user_context_);
  authenticator->auth_action_ = auth_action_;
  authenticator->has_incomplete_encryption_migration_ =
      has_incomplete_encryption_migration_;
  if (auth_action_ == StubAuthenticator::AuthAction::kAuthFailure)
    authenticator->failure_reason_ = failure_reason_;
  return authenticator;
}

void StubAuthenticatorBuilder::SetUpOldEncryption(
    bool has_incomplete_migration) {
  DCHECK_EQ(auth_action_, StubAuthenticator::AuthAction::kAuthSuccess);
  auth_action_ = StubAuthenticator::AuthAction::kOldEncryption;
  has_incomplete_encryption_migration_ = has_incomplete_migration;
}

void StubAuthenticatorBuilder::SetUpAuthFailure(
    AuthFailure::FailureReason failure_reason) {
  DCHECK_EQ(auth_action_, StubAuthenticator::AuthAction::kAuthSuccess);
  auth_action_ = StubAuthenticator::AuthAction::kAuthFailure;
  failure_reason_ = failure_reason;
}

}  // namespace ash
