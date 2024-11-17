// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/possible_username_data.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

namespace password_manager {

namespace {

constexpr char16_t kUser[] = u"user";

class IsPossibleUsernameValidTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  PossibleUsernameData possible_username_data_{
      "https://example.com/" /* submitted_signon_realm */,
      autofill::FieldRendererId(1u),
      kUser /* value */,
      base::Time::Now() /* last_change */,
      /*driver_id=*/10,
      /*autocomplete_attribute_has_username=*/false,
      /*is_likely_otp=*/false};
};

// Checks that `possible_username_data_` becomes stale over some time.
TEST_F(IsPossibleUsernameValidTest, IsPossibleUsernameStale) {
  EXPECT_FALSE(possible_username_data_.IsStale());

  // Fast forward for a little less than expiration time, but not
  // exactly to not flake the test.
  task_environment_.FastForwardBy(kSingleUsernameTimeToLive);
  EXPECT_FALSE(possible_username_data_.IsStale());

  // Fast forward more until the data becomes stale.
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(possible_username_data_.IsStale());
}

}  // namespace
}  // namespace password_manager
