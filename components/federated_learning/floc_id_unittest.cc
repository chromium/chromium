// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/floc_id.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/federated_learning/features/features.h"
#include "components/federated_learning/floc_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/federated_learning/floc.mojom.h"

namespace federated_learning {

namespace {

blink::mojom::InterestCohortPtr InterestCohortResult(
    const std::string& id,
    const std::string& version) {
  blink::mojom::InterestCohortPtr result = blink::mojom::InterestCohort::New();
  result->id = id;
  result->version = version;
  return result;
}

const base::Time kTime0 = base::Time();
const base::Time kTime1 = base::Time::FromTimeT(1);
const base::Time kTime2 = base::Time::FromTimeT(2);

}  // namespace

class FlocIdUnitTest : public testing::Test {
 public:
  FlocIdUnitTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~FlocIdUnitTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(FlocIdUnitTest, IsValid) {
  EXPECT_FALSE(FlocId().IsValid());
  EXPECT_TRUE(FlocId(0, kTime0, kTime0, 0).IsValid());
  EXPECT_TRUE(FlocId(0, kTime1, kTime2, 1).IsValid());
}

TEST_F(FlocIdUnitTest, Comparison) {
  EXPECT_EQ(FlocId(), FlocId());

  EXPECT_EQ(FlocId(0, kTime0, kTime0, 0), FlocId(0, kTime0, kTime0, 0));
  EXPECT_EQ(FlocId(0, kTime1, kTime1, 1), FlocId(0, kTime1, kTime1, 1));
  EXPECT_EQ(FlocId(0, kTime1, kTime2, 1), FlocId(0, kTime1, kTime2, 1));

  EXPECT_NE(FlocId(), FlocId(0, kTime0, kTime0, 0));
  EXPECT_NE(FlocId(0, kTime0, kTime0, 0), FlocId(1, kTime0, kTime0, 0));
  EXPECT_NE(FlocId(0, kTime0, kTime1, 0), FlocId(0, kTime1, kTime1, 0));
  EXPECT_NE(FlocId(0, kTime0, kTime0, 0), FlocId(0, kTime0, kTime0, 1));
}

TEST_F(FlocIdUnitTest, ToInterestCohortForJsApi) {
  EXPECT_EQ(InterestCohortResult("0", "chrome.1.0"),
            FlocId(0, kTime0, kTime0, 0).ToInterestCohortForJsApi());
  EXPECT_EQ(InterestCohortResult("12345", "chrome.1.0"),
            FlocId(12345, kTime0, kTime0, 0).ToInterestCohortForJsApi());
  EXPECT_EQ(InterestCohortResult("12345", "chrome.1.2"),
            FlocId(12345, kTime1, kTime1, 2).ToInterestCohortForJsApi());

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kFederatedLearningOfCohorts, {{"finch_config_version", "99"}});

  EXPECT_EQ(InterestCohortResult("0", "chrome.99.0"),
            FlocId(0, kTime0, kTime0, 0).ToInterestCohortForJsApi());
  EXPECT_EQ(InterestCohortResult("12345", "chrome.99.0"),
            FlocId(12345, kTime0, kTime0, 0).ToInterestCohortForJsApi());
  EXPECT_EQ(InterestCohortResult("12345", "chrome.99.2"),
            FlocId(12345, kTime1, kTime1, 2).ToInterestCohortForJsApi());
}

TEST_F(FlocIdUnitTest, ReadFromPrefs_DefaultInvalid) {
  TestingPrefServiceSimple prefs;
  FlocId::RegisterPrefs(prefs.registry());

  FlocId floc_id = FlocId::ReadFromPrefs(&prefs);

  EXPECT_FALSE(floc_id.IsValid());
  EXPECT_TRUE(floc_id.history_begin_time().is_null());
  EXPECT_TRUE(floc_id.history_end_time().is_null());
  EXPECT_EQ(0u, floc_id.finch_config_version());
  EXPECT_EQ(0u, floc_id.sorting_lsh_version());
  EXPECT_TRUE(floc_id.compute_time().is_null());
}

TEST_F(FlocIdUnitTest, ReadFromPrefs_SavedInvalid) {
  TestingPrefServiceSimple prefs;
  FlocId::RegisterPrefs(prefs.registry());

  prefs.ClearPref(kFlocIdValuePrefKey);
  prefs.SetTime(kFlocIdHistoryBeginTimePrefKey, base::Time::FromTimeT(1));
  prefs.SetTime(kFlocIdHistoryEndTimePrefKey, base::Time::FromTimeT(2));
  prefs.SetUint64(kFlocIdFinchConfigVersionPrefKey, 3);
  prefs.SetUint64(kFlocIdSortingLshVersionPrefKey, 4);
  prefs.SetTime(kFlocIdComputeTimePrefKey, base::Time::FromTimeT(5));

  FlocId floc_id = FlocId::ReadFromPrefs(&prefs);
  EXPECT_FALSE(floc_id.IsValid());
  EXPECT_EQ(base::Time::FromTimeT(1), floc_id.history_begin_time());
  EXPECT_EQ(base::Time::FromTimeT(2), floc_id.history_end_time());
  EXPECT_EQ(3u, floc_id.finch_config_version());
  EXPECT_EQ(4u, floc_id.sorting_lsh_version());
  EXPECT_EQ(base::Time::FromTimeT(5), floc_id.compute_time());
}

TEST_F(FlocIdUnitTest, ReadFromPrefs_SavedValid) {
  TestingPrefServiceSimple prefs;
  FlocId::RegisterPrefs(prefs.registry());

  prefs.SetUint64(kFlocIdValuePrefKey, 123);
  prefs.SetTime(kFlocIdHistoryBeginTimePrefKey, base::Time::FromTimeT(1));
  prefs.SetTime(kFlocIdHistoryEndTimePrefKey, base::Time::FromTimeT(2));
  prefs.SetUint64(kFlocIdFinchConfigVersionPrefKey, 3);
  prefs.SetUint64(kFlocIdSortingLshVersionPrefKey, 4);
  prefs.SetTime(kFlocIdComputeTimePrefKey, base::Time::FromTimeT(5));

  FlocId floc_id = FlocId::ReadFromPrefs(&prefs);
  EXPECT_TRUE(floc_id.IsValid());
  EXPECT_EQ(base::Time::FromTimeT(1), floc_id.history_begin_time());
  EXPECT_EQ(base::Time::FromTimeT(2), floc_id.history_end_time());
  EXPECT_EQ(3u, floc_id.finch_config_version());
  EXPECT_EQ(4u, floc_id.sorting_lsh_version());
  EXPECT_EQ(base::Time::FromTimeT(5), floc_id.compute_time());
  EXPECT_EQ(InterestCohortResult("123", "chrome.3.4"),
            floc_id.ToInterestCohortForJsApi());
}

TEST_F(FlocIdUnitTest, SaveToPrefs_InvalidFloc) {
  TestingPrefServiceSimple prefs;
  FlocId::RegisterPrefs(prefs.registry());

  FlocId floc_id;
  floc_id.SaveToPrefs(&prefs);

  EXPECT_FALSE(prefs.HasPrefPath(kFlocIdValuePrefKey));
  EXPECT_TRUE(prefs.HasPrefPath(kFlocIdHistoryBeginTimePrefKey));
  EXPECT_TRUE(prefs.HasPrefPath(kFlocIdHistoryEndTimePrefKey));
  EXPECT_TRUE(prefs.HasPrefPath(kFlocIdFinchConfigVersionPrefKey));
  EXPECT_TRUE(prefs.HasPrefPath(kFlocIdSortingLshVersionPrefKey));
  EXPECT_TRUE(prefs.HasPrefPath(kFlocIdComputeTimePrefKey));

  EXPECT_EQ(0u, prefs.GetUint64(kFlocIdValuePrefKey));
  EXPECT_TRUE(prefs.GetTime(kFlocIdHistoryBeginTimePrefKey).is_null());
  EXPECT_TRUE(prefs.GetTime(kFlocIdHistoryEndTimePrefKey).is_null());
  EXPECT_EQ(1u, prefs.GetUint64(kFlocIdFinchConfigVersionPrefKey));
  EXPECT_EQ(0u, prefs.GetUint64(kFlocIdSortingLshVersionPrefKey));
  EXPECT_EQ(base::Time::Now(), prefs.GetTime(kFlocIdComputeTimePrefKey));
}

TEST_F(FlocIdUnitTest, SaveToPrefs_ValidFloc) {
  TestingPrefServiceSimple prefs;
  FlocId::RegisterPrefs(prefs.registry());

  FlocId floc_id =
      FlocId(123, base::Time::FromTimeT(1), base::Time::FromTimeT(2), 3);
  floc_id.SaveToPrefs(&prefs);

  EXPECT_TRUE(prefs.HasPrefPath(kFlocIdValuePrefKey));
  EXPECT_TRUE(prefs.HasPrefPath(kFlocIdHistoryBeginTimePrefKey));
  EXPECT_TRUE(prefs.HasPrefPath(kFlocIdHistoryEndTimePrefKey));
  EXPECT_TRUE(prefs.HasPrefPath(kFlocIdFinchConfigVersionPrefKey));
  EXPECT_TRUE(prefs.HasPrefPath(kFlocIdSortingLshVersionPrefKey));
  EXPECT_TRUE(prefs.HasPrefPath(kFlocIdComputeTimePrefKey));

  EXPECT_EQ(123u, prefs.GetUint64(kFlocIdValuePrefKey));
  EXPECT_EQ(base::Time::FromTimeT(1),
            prefs.GetTime(kFlocIdHistoryBeginTimePrefKey));
  EXPECT_EQ(base::Time::FromTimeT(2),
            prefs.GetTime(kFlocIdHistoryEndTimePrefKey));
  EXPECT_EQ(1u, prefs.GetUint64(kFlocIdFinchConfigVersionPrefKey));
  EXPECT_EQ(3u, prefs.GetUint64(kFlocIdSortingLshVersionPrefKey));
  EXPECT_EQ(base::Time::Now(), prefs.GetTime(kFlocIdComputeTimePrefKey));
}

TEST_F(FlocIdUnitTest, InvalidateIdAndSaveToPrefs) {
  TestingPrefServiceSimple prefs;
  FlocId::RegisterPrefs(prefs.registry());

  FlocId floc_id =
      FlocId(123, base::Time::FromTimeT(1), base::Time::FromTimeT(2), 3);
  floc_id.SaveToPrefs(&prefs);

  floc_id.InvalidateIdAndSaveToPrefs(&prefs);
  EXPECT_FALSE(floc_id.IsValid());
  EXPECT_FALSE(prefs.HasPrefPath(kFlocIdValuePrefKey));

  EXPECT_EQ(base::Time::FromTimeT(1),
            prefs.GetTime(kFlocIdHistoryBeginTimePrefKey));
  EXPECT_EQ(base::Time::FromTimeT(2),
            prefs.GetTime(kFlocIdHistoryEndTimePrefKey));
  EXPECT_EQ(1u, prefs.GetUint64(kFlocIdFinchConfigVersionPrefKey));
  EXPECT_EQ(3u, prefs.GetUint64(kFlocIdSortingLshVersionPrefKey));
  EXPECT_EQ(base::Time::Now(), prefs.GetTime(kFlocIdComputeTimePrefKey));
}

TEST_F(FlocIdUnitTest, ResetComputeTimeAndSaveToPrefs) {
  TestingPrefServiceSimple prefs;
  FlocId::RegisterPrefs(prefs.registry());

  FlocId floc_id =
      FlocId(123, base::Time::FromTimeT(1), base::Time::FromTimeT(2), 3);
  floc_id.SaveToPrefs(&prefs);
  EXPECT_EQ(base::Time::Now(), prefs.GetTime(kFlocIdComputeTimePrefKey));

  floc_id.ResetComputeTimeAndSaveToPrefs(base::Time::FromTimeT(4), &prefs);
  EXPECT_EQ(base::Time::FromTimeT(4), prefs.GetTime(kFlocIdComputeTimePrefKey));
}

}  // namespace federated_learning
