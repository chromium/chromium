// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/floc_id.h"

#include "components/federated_learning/floc_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace federated_learning {

const base::Time kTime0 = base::Time();
const base::Time kTime1 = base::Time::FromTimeT(1);
const base::Time kTime2 = base::Time::FromTimeT(2);

TEST(FlocIdTest, IsValid) {
  EXPECT_FALSE(FlocId().IsValid());
  EXPECT_TRUE(FlocId(0, kTime0, kTime0, 0).IsValid());
  EXPECT_TRUE(FlocId(0, kTime1, kTime2, 1).IsValid());
}

TEST(FlocIdTest, Comparison) {
  EXPECT_EQ(FlocId(), FlocId());

  EXPECT_EQ(FlocId(0, kTime0, kTime0, 0), FlocId(0, kTime0, kTime0, 0));
  EXPECT_EQ(FlocId(0, kTime1, kTime1, 1), FlocId(0, kTime1, kTime1, 1));
  EXPECT_EQ(FlocId(0, kTime1, kTime2, 1), FlocId(0, kTime1, kTime2, 1));

  EXPECT_NE(FlocId(), FlocId(0, kTime0, kTime0, 0));
  EXPECT_NE(FlocId(0, kTime0, kTime0, 0), FlocId(1, kTime0, kTime0, 0));
  EXPECT_NE(FlocId(0, kTime0, kTime1, 0), FlocId(0, kTime1, kTime1, 0));
  EXPECT_NE(FlocId(0, kTime0, kTime0, 0), FlocId(0, kTime0, kTime0, 1));
}

TEST(FlocIdTest, ToStringForJsApi) {
  EXPECT_EQ("0.1.0", FlocId(0, kTime0, kTime0, 0).ToStringForJsApi());
  EXPECT_EQ("12345.1.0", FlocId(12345, kTime0, kTime0, 0).ToStringForJsApi());
  EXPECT_EQ("12345.1.2", FlocId(12345, kTime1, kTime1, 2).ToStringForJsApi());
}

TEST(FlocIdTest, ReadFromPrefs_DefaultInvalid) {
  TestingPrefServiceSimple local_state;
  FlocId::RegisterPrefs(local_state.registry());

  FlocId floc_id = FlocId::ReadFromPrefs(&local_state);
  EXPECT_FALSE(floc_id.IsValid());

  base::Time compute_time = FlocId::ReadComputeTimeFromPrefs(&local_state);
  EXPECT_TRUE(compute_time.is_null());
}

TEST(FlocIdTest, ReadFromPrefs_SavedInvalid) {
  TestingPrefServiceSimple local_state;
  FlocId::RegisterPrefs(local_state.registry());

  local_state.ClearPref(kFlocIdValuePrefKey);
  local_state.SetTime(kFlocIdHistoryBeginTimePrefKey, base::Time::FromTimeT(1));
  local_state.SetTime(kFlocIdHistoryEndTimePrefKey, base::Time::FromTimeT(2));
  local_state.SetUint64(kFlocIdSortingLshVersionPrefKey, 2);

  FlocId floc_id = FlocId::ReadFromPrefs(&local_state);
  EXPECT_FALSE(floc_id.IsValid());

  local_state.SetTime(kFlocIdComputeTimePrefKey, base::Time());
  base::Time compute_time = FlocId::ReadComputeTimeFromPrefs(&local_state);
  EXPECT_TRUE(compute_time.is_null());
}

TEST(FlocIdTest, ReadFromPrefs_SavedValid) {
  TestingPrefServiceSimple local_state;
  FlocId::RegisterPrefs(local_state.registry());

  local_state.SetUint64(kFlocIdValuePrefKey, 123);
  local_state.SetTime(kFlocIdHistoryBeginTimePrefKey, base::Time::FromTimeT(1));
  local_state.SetTime(kFlocIdHistoryEndTimePrefKey, base::Time::FromTimeT(2));
  local_state.SetUint64(kFlocIdSortingLshVersionPrefKey, 2);

  FlocId floc_id = FlocId::ReadFromPrefs(&local_state);
  EXPECT_EQ(floc_id,
            FlocId(123, base::Time::FromTimeT(1), base::Time::FromTimeT(2), 2));

  local_state.SetTime(kFlocIdComputeTimePrefKey, base::Time::FromTimeT(3));
  base::Time compute_time = FlocId::ReadComputeTimeFromPrefs(&local_state);
  EXPECT_EQ(compute_time, base::Time::FromTimeT(3));
}

TEST(FlocIdTest, SaveToPrefs_InvalidFloc) {
  TestingPrefServiceSimple local_state;
  FlocId::RegisterPrefs(local_state.registry());

  FlocId floc_id;
  floc_id.SaveToPrefs(&local_state);

  EXPECT_FALSE(local_state.HasPrefPath(kFlocIdValuePrefKey));
  EXPECT_FALSE(local_state.HasPrefPath(kFlocIdHistoryBeginTimePrefKey));
  EXPECT_FALSE(local_state.HasPrefPath(kFlocIdHistoryEndTimePrefKey));
  EXPECT_FALSE(local_state.HasPrefPath(kFlocIdSortingLshVersionPrefKey));

  EXPECT_EQ(0u, local_state.GetUint64(kFlocIdValuePrefKey));
  EXPECT_TRUE(local_state.GetTime(kFlocIdHistoryBeginTimePrefKey).is_null());
  EXPECT_TRUE(local_state.GetTime(kFlocIdHistoryEndTimePrefKey).is_null());
  EXPECT_EQ(0u, local_state.GetUint64(kFlocIdSortingLshVersionPrefKey));

  FlocId::SaveComputeTimeToPrefs(base::Time(), &local_state);
  EXPECT_TRUE(local_state.GetTime(kFlocIdComputeTimePrefKey).is_null());
}

TEST(FlocIdTest, SaveToPrefs_ValidFloc) {
  TestingPrefServiceSimple local_state;
  FlocId::RegisterPrefs(local_state.registry());

  FlocId floc_id =
      FlocId(123, base::Time::FromTimeT(1), base::Time::FromTimeT(2), 2);
  floc_id.SaveToPrefs(&local_state);

  EXPECT_TRUE(local_state.HasPrefPath(kFlocIdValuePrefKey));
  EXPECT_TRUE(local_state.HasPrefPath(kFlocIdHistoryBeginTimePrefKey));
  EXPECT_TRUE(local_state.HasPrefPath(kFlocIdHistoryEndTimePrefKey));
  EXPECT_TRUE(local_state.HasPrefPath(kFlocIdSortingLshVersionPrefKey));

  EXPECT_EQ(123u, local_state.GetUint64(kFlocIdValuePrefKey));
  EXPECT_EQ(base::Time::FromTimeT(1),
            local_state.GetTime(kFlocIdHistoryBeginTimePrefKey));
  EXPECT_EQ(base::Time::FromTimeT(2),
            local_state.GetTime(kFlocIdHistoryEndTimePrefKey));
  EXPECT_EQ(2u, local_state.GetUint64(kFlocIdSortingLshVersionPrefKey));

  FlocId::SaveComputeTimeToPrefs(base::Time::FromTimeT(3), &local_state);
  EXPECT_EQ(base::Time::FromTimeT(3),
            local_state.GetTime(kFlocIdComputeTimePrefKey));
}

}  // namespace federated_learning
