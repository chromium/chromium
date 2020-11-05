// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/mock_async_method_caller.h"

using ::testing::Invoke;
using ::testing::WithArgs;
using ::testing::_;

namespace cryptohome {

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
}

void MockAsyncMethodCaller::FakeGetSanitizedUsername(DataCallback callback) {
  std::move(callback).Run(success_, kFakeSanitizedUsername);
}

void MockAsyncMethodCaller::FakeEnterpriseChallenge(DataCallback callback) {
  std::move(callback).Run(success_, kFakeChallengeResponse);
}

}  // namespace cryptohome
