// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/possible_username_data.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::Time;
using base::TimeDelta;

namespace password_manager {

namespace {

class IsPossibleUsernameValidTest : public testing::Test {
 public:
  IsPossibleUsernameValidTest()
      : possible_username_data_(
            "https://example.com/" /* submitted_signon_realm */,
            1u /* renderer_id */,
            ASCIIToUTF16("username") /* value */,
            base::Time::Now() /* last_change */,
            10 /* driver_id */) {}

 protected:
  PossibleUsernameData possible_username_data_;
};

TEST_F(IsPossibleUsernameValidTest, Valid) {
  EXPECT_TRUE(IsPossibleUsernameValid(possible_username_data_,
                                      possible_username_data_.signon_realm,
                                      possible_username_data_.last_change));
}

// Check that if time delta between last change and submission is more than 60
// seconds, than data is invalid.
TEST_F(IsPossibleUsernameValidTest, TimeDeltaBeforeLastChangeAndSubmission) {
  Time valid_submission_time = possible_username_data_.last_change +
                               kMaxDelayBetweenTypingUsernameAndSubmission;
  Time invalid_submission_time =
      valid_submission_time + TimeDelta::FromSeconds(1);
  EXPECT_TRUE(IsPossibleUsernameValid(possible_username_data_,
                                      possible_username_data_.signon_realm,
                                      valid_submission_time));
  EXPECT_FALSE(IsPossibleUsernameValid(possible_username_data_,
                                       possible_username_data_.signon_realm,
                                       invalid_submission_time));
}

TEST_F(IsPossibleUsernameValidTest, SignonRealm) {
  EXPECT_FALSE(IsPossibleUsernameValid(possible_username_data_,
                                       "https://m.example.com/",
                                       possible_username_data_.last_change));

  EXPECT_FALSE(IsPossibleUsernameValid(possible_username_data_,
                                       "https://google.com/",
                                       possible_username_data_.last_change));
}

TEST_F(IsPossibleUsernameValidTest, PossibleUsernameValue) {
  PossibleUsernameData possible_username_data = possible_username_data_;

  // White spaces are not allowed in username.
  possible_username_data.value = ASCIIToUTF16("user name");
  EXPECT_FALSE(IsPossibleUsernameValid(possible_username_data,
                                       possible_username_data.signon_realm,
                                       possible_username_data.last_change));

  // New lines are not allowed in username.
  possible_username_data.value = ASCIIToUTF16("user\nname");
  EXPECT_FALSE(IsPossibleUsernameValid(possible_username_data,
                                       possible_username_data.signon_realm,
                                       possible_username_data.last_change));

  // Digits and special characters are ok.
  possible_username_data.value = ASCIIToUTF16("User_name1234!+&%#\'\"@");
  EXPECT_TRUE(IsPossibleUsernameValid(possible_username_data,
                                      possible_username_data.signon_realm,
                                      possible_username_data.last_change));
}

}  // namespace
}  // namespace password_manager
