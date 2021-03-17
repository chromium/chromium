// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/possible_username_data.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace password_manager {

namespace {

constexpr char16_t kUser[] = u"user";

class IsPossibleUsernameValidTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  PossibleUsernameData possible_username_data_{
      "https://example.com/" /* submitted_signon_realm */,
      autofill::FieldRendererId(1u), kUser /* value */,
      base::Time::Now() /* last_change */, 10 /* driver_id */};
};

TEST_F(IsPossibleUsernameValidTest, Valid) {
  EXPECT_TRUE(IsPossibleUsernameValid(
      possible_username_data_, possible_username_data_.signon_realm, {kUser}));
}

// Check that if time delta between last change and submission is more than 60
// seconds, than data is invalid.
TEST_F(IsPossibleUsernameValidTest, TimeDeltaBeforeLastChangeAndSubmission) {
  task_environment_.FastForwardBy(kMaxDelayBetweenTypingUsernameAndSubmission);
  EXPECT_TRUE(IsPossibleUsernameValid(
      possible_username_data_, possible_username_data_.signon_realm, {kUser}));
  task_environment_.FastForwardBy(TimeDelta::FromSeconds(1));
  EXPECT_FALSE(IsPossibleUsernameValid(
      possible_username_data_, possible_username_data_.signon_realm, {kUser}));
}

TEST_F(IsPossibleUsernameValidTest, SignonRealm) {
  EXPECT_FALSE(IsPossibleUsernameValid(possible_username_data_,
                                       "https://m.example.com/", {kUser}));

  EXPECT_FALSE(IsPossibleUsernameValid(possible_username_data_,
                                       "https://google.com/", {kUser}));
}

TEST_F(IsPossibleUsernameValidTest, PossibleUsernameValue) {
  // Different capitalization is okay.
  EXPECT_TRUE(IsPossibleUsernameValid(possible_username_data_,
                                      possible_username_data_.signon_realm,
                                      {u"USER"}));
  // Different email hosts are okay.
  EXPECT_TRUE(IsPossibleUsernameValid(possible_username_data_,
                                      possible_username_data_.signon_realm,
                                      {u"user@gmail.com"}));

  // Other usernames are okay.
  EXPECT_TRUE(IsPossibleUsernameValid(possible_username_data_,
                                      possible_username_data_.signon_realm,
                                      {kUser, u"alice"}));

  // No usernames are not okay.
  EXPECT_FALSE(IsPossibleUsernameValid(
      possible_username_data_, possible_username_data_.signon_realm, {}));

  // Completely different usernames are not okay.
  EXPECT_FALSE(IsPossibleUsernameValid(possible_username_data_,
                                       possible_username_data_.signon_realm,
                                       {u"alice", u"bob"}));
}

}  // namespace
}  // namespace password_manager
