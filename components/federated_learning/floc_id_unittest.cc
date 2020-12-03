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
  TestingPrefServiceSimple prefs;
  FlocId::RegisterPrefs(prefs.registry());

  FlocId floc_id = FlocId::ReadFromPrefs(&prefs);
  EXPECT_FALSE(floc_id.IsValid());

  base::Time compute_time = FlocId::ReadComputeTimeFromPrefs(&prefs);
  EXPECT_TRUE(compute_time.is_null());
}

TEST(FlocIdTest, ReadFromPrefs_SavedInvalid) {
  TestingPrefServiceSimple prefs;
  FlocId::RegisterPrefs(prefs.registry());

  prefs.ClearPref(kFlocIdValuePrefKey);
  prefs.SetTime(kFlocIdHistoryBeginTimePrefKey, base::Time::FromTimeT(1));
  prefs.SetTime(kFlocIdHistoryEndTimePrefKey, base::Time::FromTimeT(2));
  prefs.SetUint64(kFlocIdSortingLshVersionPrefKey, 2);

  FlocId floc_id = FlocId::ReadFromPrefs(&prefs);
  EXPECT_FALSE(floc_id.IsValid());

  prefs.SetTime(kFlocIdComputeTimePrefKey, base::Time());
  base::Time compute_time = FlocId::ReadComputeTimeFromPrefs(&prefs);
  EXPECT_TRUE(compute_time.is_null());
}

TEST(FlocIdTest, ReadFromPrefs_SavedValid) {
  TestingPrefServiceSimple prefs;
  FlocId::RegisterPrefs(prefs.registry());

  prefs.SetUint64(kFlocIdValuePrefKey, 123);
  prefs.SetTime(kFlocIdHistoryBeginTimePrefKey, base::Time::FromTimeT(1));
  prefs.SetTime(kFlocIdHistoryEndTimePrefKey, base::Time::FromTimeT(2));
  prefs.SetUint64(kFlocIdSortingLshVersionPrefKey, 2);

  FlocId floc_id = FlocId::ReadFromPrefs(&prefs);
  EXPECT_EQ(floc_id,
            FlocId(123, base::Time::FromTimeT(1), base::Time::FromTimeT(2), 2));

  prefs.SetTime(kFlocIdComputeTimePrefKey, base::Time::FromTimeT(3));
  base::Time compute_time = FlocId::ReadComputeTimeFromPrefs(&prefs);
  EXPECT_EQ(compute_time, base::Time::FromTimeT(3));
}

TEST(FlocIdTest, SaveToPrefs_InvalidFloc) {
  TestingPrefServiceSimple prefs;
  FlocId::RegisterPrefs(prefs.registry());

  FlocId floc_id;
  floc_id.SaveToPrefs(&prefs);

  EXPECT_FALSE(prefs.HasPrefPath(kFlocIdValuePrefKey));
  EXPECT_FALSE(prefs.HasPrefPath(kFlocIdHistoryBeginTimePrefKey));
  EXPECT_FALSE(prefs.HasPrefPath(kFlocIdHistoryEndTimePrefKey));
  EXPECT_FALSE(prefs.HasPrefPath(kFlocIdSortingLshVersionPrefKey));

  EXPECT_EQ(0u, prefs.GetUint64(kFlocIdValuePrefKey));
  EXPECT_TRUE(prefs.GetTime(kFlocIdHistoryBeginTimePrefKey).is_null());
  EXPECT_TRUE(prefs.GetTime(kFlocIdHistoryEndTimePrefKey).is_null());
  EXPECT_EQ(0u, prefs.GetUint64(kFlocIdSortingLshVersionPrefKey));

  FlocId::SaveComputeTimeToPrefs(base::Time(), &prefs);
  EXPECT_TRUE(prefs.GetTime(kFlocIdComputeTimePrefKey).is_null());
}

TEST(FlocIdTest, SaveToPrefs_ValidFloc) {
  TestingPrefServiceSimple prefs;
  FlocId::RegisterPrefs(prefs.registry());

  FlocId floc_id =
      FlocId(123, base::Time::FromTimeT(1), base::Time::FromTimeT(2), 2);
  floc_id.SaveToPrefs(&prefs);

  EXPECT_TRUE(prefs.HasPrefPath(kFlocIdValuePrefKey));
  EXPECT_TRUE(prefs.HasPrefPath(kFlocIdHistoryBeginTimePrefKey));
  EXPECT_TRUE(prefs.HasPrefPath(kFlocIdHistoryEndTimePrefKey));
  EXPECT_TRUE(prefs.HasPrefPath(kFlocIdSortingLshVersionPrefKey));

  EXPECT_EQ(123u, prefs.GetUint64(kFlocIdValuePrefKey));
  EXPECT_EQ(base::Time::FromTimeT(1),
            prefs.GetTime(kFlocIdHistoryBeginTimePrefKey));
  EXPECT_EQ(base::Time::FromTimeT(2),
            prefs.GetTime(kFlocIdHistoryEndTimePrefKey));
  EXPECT_EQ(2u, prefs.GetUint64(kFlocIdSortingLshVersionPrefKey));

  FlocId::SaveComputeTimeToPrefs(base::Time::FromTimeT(3), &prefs);
  EXPECT_EQ(base::Time::FromTimeT(3), prefs.GetTime(kFlocIdComputeTimePrefKey));
}

}  // namespace federated_learning
