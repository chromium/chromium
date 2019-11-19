// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/user_demographics.h"

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/user_demographics.pb.h"

namespace syncer {

TEST(UserDemographicsTest, UserDemographicsResult_ForValue) {
  int user_birth_year = 1982;
  metrics::UserDemographicsProto_Gender user_gender =
      metrics::UserDemographicsProto::GENDER_MALE;

  UserDemographics user_demographics;
  user_demographics.birth_year = user_birth_year;
  user_demographics.gender = user_gender;
  UserDemographicsResult user_demographics_result =
      UserDemographicsResult::ForValue(std::move(user_demographics));

  EXPECT_TRUE(user_demographics_result.IsSuccess());
  EXPECT_EQ(UserDemographicsStatus::kSuccess,
            user_demographics_result.status());
  EXPECT_EQ(user_birth_year, user_demographics_result.value().birth_year);
  EXPECT_EQ(user_gender, user_demographics_result.value().gender);
}

TEST(UserDemographicsTest, UserDemographicsResult_ForStatus) {
  UserDemographicsStatus error_status =
      UserDemographicsStatus::kIneligibleDemographicsData;
  UserDemographicsResult user_demographics_result =
      UserDemographicsResult::ForStatus(error_status);

  EXPECT_FALSE(user_demographics_result.IsSuccess());
  EXPECT_EQ(error_status, user_demographics_result.status());
}

}  // namespace syncer
