// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/mock_homedir_methods.h"

#include <vector>

#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"

using ::testing::Invoke;
using ::testing::WithArgs;
using ::testing::_;

namespace cryptohome {

MockHomedirMethods::MockHomedirMethods() = default;
MockHomedirMethods::~MockHomedirMethods() = default;

void MockHomedirMethods::SetUp(bool success, MountError return_code) {
  success_ = success;
  return_code_ = return_code;
  ON_CALL(*this, CheckKeyEx(_, _, _, _))
      .WillByDefault(
          WithArgs<3>(Invoke(this, &MockHomedirMethods::DoCallback)));
  ON_CALL(*this, AddKeyEx(_, _, _, _))
      .WillByDefault(
          WithArgs<3>(Invoke(this, &MockHomedirMethods::DoAddKeyCallback)));
  ON_CALL(*this, RemoveKeyEx(_, _, _, _)).WillByDefault(
      WithArgs<3>(Invoke(this, &MockHomedirMethods::DoCallback)));
}

void MockHomedirMethods::DoCallback(Callback callback) {
  std::move(callback).Run(success_, return_code_);
}

void MockHomedirMethods::DoAddKeyCallback(Callback callback) {
  std::move(callback).Run(success_, return_code_);
}

}  // namespace cryptohome
