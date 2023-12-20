// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_SUPERVISED_USER_LOGIN_DELEGATE_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_SUPERVISED_USER_LOGIN_DELEGATE_H_

#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"

// Provides login support for supervised user accounts in Crosier tests.
class SupervisedUserLoginDelegate : public CustomGaiaLoginDelegate {
 public:
  // Type of supervised user account being used to log in.
  enum class SupervisedUserType {
    kUnicorn,
    kGeller,
    kGriffin,
  };

  SupervisedUserLoginDelegate() = default;
  SupervisedUserLoginDelegate(const SupervisedUserLoginDelegate&) = delete;
  SupervisedUserLoginDelegate& operator=(const SupervisedUserLoginDelegate&) =
      delete;
  ~SupervisedUserLoginDelegate() = default;

  // CustomGaiaLoginDelegate:
  void DoCustomGaiaLogin(std::string& username) override;

  void set_user_type(SupervisedUserType user_type) { user_type_ = user_type; }

 private:
  SupervisedUserType user_type_ = SupervisedUserType::kUnicorn;
};

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_SUPERVISED_USER_LOGIN_DELEGATE_H_
