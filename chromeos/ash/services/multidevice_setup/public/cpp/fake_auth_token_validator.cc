// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_auth_token_validator.h"

namespace ash {

namespace multidevice_setup {

FakeAuthTokenValidator::FakeAuthTokenValidator() = default;

FakeAuthTokenValidator::~FakeAuthTokenValidator() = default;

bool FakeAuthTokenValidator::IsAuthTokenValid(const std::string& auth_token) {
  if (!expected_auth_token_)
    return false;

  return *expected_auth_token_ == auth_token;
}

}  // namespace multidevice_setup

}  // namespace ash
