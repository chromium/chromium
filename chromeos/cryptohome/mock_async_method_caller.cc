// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/mock_async_method_caller.h"

using ::testing::Invoke;
using ::testing::WithArgs;
using ::testing::_;

namespace cryptohome {

const char MockAsyncMethodCaller::kFakeAttestationEnrollRequest[] = "enrollreq";
const char MockAsyncMethodCaller::kFakeAttestationCertRequest[] = "certreq";
const char MockAsyncMethodCaller::kFakeAttestationCert[] = "cert";
const char MockAsyncMethodCaller::kFakeSanitizedUsername[] = "01234567890ABC";
const char MockAsyncMethodCaller::kFakeChallengeResponse[] =
    "challenge_response";

MockAsyncMethodCaller::MockAsyncMethodCaller()
    : success_(false), return_code_(cryptohome::MOUNT_ERROR_NONE) {
}

MockAsyncMethodCaller::~MockAsyncMethodCaller() = default;

void MockAsyncMethodCaller::SetUp(bool success, MountError return_code) {
  success_ = success;
  return_code_ = return_code;
  ON_CALL(*this, AsyncTpmAttestationCreateEnrollRequest(_, _))
      .WillByDefault(
          WithArgs<1>(Invoke(this,
                             &MockAsyncMethodCaller::FakeCreateEnrollRequest)));
  ON_CALL(*this, AsyncTpmAttestationEnroll(_, _, _))
      .WillByDefault(
          WithArgs<2>(Invoke(this, &MockAsyncMethodCaller::DoCallback)));
  ON_CALL(*this, AsyncTpmAttestationCreateCertRequest(_, _, _, _, _))
      .WillByDefault(
          WithArgs<4>(Invoke(this,
                             &MockAsyncMethodCaller::FakeCreateCertRequest)));
  ON_CALL(*this, AsyncTpmAttestationFinishCertRequest(_, _, _, _, _))
      .WillByDefault(
          WithArgs<4>(Invoke(this,
                             &MockAsyncMethodCaller::FakeFinishCertRequest)));
  ON_CALL(*this,
          TpmAttestationSignEnterpriseChallenge(_, _, _, _, _, _, _, _, _))
      .WillByDefault(WithArgs<8>(
          Invoke(this, &MockAsyncMethodCaller::FakeEnterpriseChallenge)));
  ON_CALL(*this, TpmAttestationRegisterKey(_, _, _, _))
      .WillByDefault(
          WithArgs<3>(Invoke(this, &MockAsyncMethodCaller::DoCallback)));
}

void MockAsyncMethodCaller::DoCallback(Callback callback) {
  callback.Run(success_, return_code_);
}

void MockAsyncMethodCaller::FakeCreateEnrollRequest(
    const DataCallback& callback) {
  callback.Run(success_, kFakeAttestationEnrollRequest);
}

void MockAsyncMethodCaller::FakeCreateCertRequest(
    const DataCallback& callback) {
  callback.Run(success_, kFakeAttestationCertRequest);
}

void MockAsyncMethodCaller::FakeFinishCertRequest(
    const DataCallback& callback) {
  callback.Run(success_, kFakeAttestationCert);
}

void MockAsyncMethodCaller::FakeGetSanitizedUsername(
    const DataCallback& callback) {
  callback.Run(success_, kFakeSanitizedUsername);
}

void MockAsyncMethodCaller::FakeEnterpriseChallenge(
    const DataCallback& callback) {
  callback.Run(success_, kFakeChallengeResponse);
}

}  // namespace cryptohome
