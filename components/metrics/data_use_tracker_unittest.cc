// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/data_use_tracker.h"

#include "base/time/time.h"
#include "base/values.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

const char kTodayStr[] = "2016-03-16";
const char kYesterdayStr[] = "2016-03-15";
const char kExpiredDateStr1[] = "2016-03-09";
const char kExpiredDateStr2[] = "2016-03-01";

class TestDataUsePrefService : public TestingPrefServiceSimple {
 public:
  TestDataUsePrefService() { DataUseTracker::RegisterPrefs(registry()); }

  TestDataUsePrefService(const TestDataUsePrefService&) = delete;
  TestDataUsePrefService& operator=(const TestDataUsePrefService&) = delete;

  void ClearDataUsePrefs() {
    ClearPref(metrics::prefs::kUserCellDataUse);
    ClearPref(metrics::prefs::kUmaCellDataUse);
  }
};

class FakeDataUseTracker : public DataUseTracker {
 public:
  FakeDataUseTracker(PrefService* local_state) : DataUseTracker(local_state) {}

  FakeDataUseTracker(const FakeDataUseTracker&) = delete;
  FakeDataUseTracker& operator=(const FakeDataUseTracker&) = delete;

  base::Time GetCurrentMeasurementDate() const override {
    base::Time today_for_test;
    EXPECT_TRUE(base::Time::FromUTCString(kTodayStr, &today_for_test));
    return today_for_test;
  }

  std::string GetCurrentMeasurementDateAsString() const override {
    return kTodayStr;
  }
};

// Sets up data usage prefs with mock values so that UMA traffic is above the
// allowed ratio.
void SetPrefTestValuesOverRatio(PrefService* local_state) {
  base::Value::Dict user_pref_dict;
  user_pref_dict.Set(kTodayStr, 2 * 100 * 1024);
  user_pref_dict.Set(kYesterdayStr, 2 * 100 * 1024);
  user_pref_dict.Set(kExpiredDateStr1, 2 * 100 * 1024);
  user_pref_dict.Set(kExpiredDateStr2, 2 * 100 * 1024);
  local_state->SetDict(prefs::kUserCellDataUse, std::move(user_pref_dict));

  base::Value::Dict uma_pref_dict;
  uma_pref_dict.Set(kTodayStr, 50 * 1024);
  uma_pref_dict.Set(kYesterdayStr, 50 * 1024);
  uma_pref_dict.Set(kExpiredDateStr1, 50 * 1024);
  uma_pref_dict.Set(kExpiredDateStr2, 50 * 1024);
  local_state->SetDict(prefs::kUmaCellDataUse, std::move(uma_pref_dict));
}

// Sets up data usage prefs with mock values which can be valid.
void SetPrefTestValuesValidRatio(PrefService* local_state) {
  base::Value::Dict user_pref_dict;
  user_pref_dict.Set(kTodayStr, 100 * 100 * 1024);
  user_pref_dict.Set(kYesterdayStr, 100 * 100 * 1024);
  user_pref_dict.Set(kExpiredDateStr1, 100 * 100 * 1024);
  user_pref_dict.Set(kExpiredDateStr2, 100 * 100 * 1024);
  local_state->SetDict(prefs::kUserCellDataUse, std::move(user_pref_dict));

  // Should be 4% of user traffic
  base::Value::Dict uma_pref_dict;
  uma_pref_dict.Set(kTodayStr, 4 * 100 * 1024);
  uma_pref_dict.Set(kYesterdayStr, 4 * 100 * 1024);
  uma_pref_dict.Set(kExpiredDateStr1, 4 * 100 * 1024);
  uma_pref_dict.Set(kExpiredDateStr2, 4 * 100 * 1024);
  local_state->SetDict(prefs::kUmaCellDataUse, std::move(uma_pref_dict));
}

}  // namespace

TEST(DataUseTrackerTest, CheckUpdateUsagePref) {
  TestDataUsePrefService local_state;
  FakeDataUseTracker data_use_tracker(&local_state);
  local_state.ClearDataUsePrefs();

  data_use_tracker.UpdateMetricsUsagePrefsInternal(2 * 100 * 1024, true, false);
  EXPECT_EQ(2 * 100 * 1024,
            local_state.GetDict(prefs::kUserCellDataUse).FindInt(kTodayStr));
  EXPECT_FALSE(local_state.GetDict(prefs::kUmaCellDataUse).FindInt(kTodayStr));

  data_use_tracker.UpdateMetricsUsagePrefsInternal(100 * 1024, true, true);
  EXPECT_EQ(3 * 100 * 1024,
            local_state.GetDict(prefs::kUserCellDataUse).FindInt(kTodayStr));
  EXPECT_EQ(100 * 1024,
            local_state.GetDict(prefs::kUmaCellDataUse).FindInt(kTodayStr));
}

TEST(DataUseTrackerTest, CheckRemoveExpiredEntries) {
  TestDataUsePrefService local_state;
  FakeDataUseTracker data_use_tracker(&local_state);
  local_state.ClearDataUsePrefs();
  SetPrefTestValuesOverRatio(&local_state);
  data_use_tracker.RemoveExpiredEntries();

  EXPECT_FALSE(
      local_state.GetDict(prefs::kUserCellDataUse).FindInt(kExpiredDateStr1));
  EXPECT_FALSE(
      local_state.GetDict(prefs::kUmaCellDataUse).FindInt(kExpiredDateStr1));

  EXPECT_FALSE(
      local_state.GetDict(prefs::kUserCellDataUse).FindInt(kExpiredDateStr2));
  EXPECT_FALSE(
      local_state.GetDict(prefs::kUmaCellDataUse).FindInt(kExpiredDateStr2));

  EXPECT_EQ(2 * 100 * 1024,
            local_state.GetDict(prefs::kUserCellDataUse).FindInt(kTodayStr));
  EXPECT_EQ(50 * 1024,
            local_state.GetDict(prefs::kUmaCellDataUse).FindInt(kTodayStr));

  EXPECT_EQ(
      2 * 100 * 1024,
      local_state.GetDict(prefs::kUserCellDataUse).FindInt(kYesterdayStr));
  EXPECT_EQ(50 * 1024,
            local_state.GetDict(prefs::kUmaCellDataUse).FindInt(kYesterdayStr));
}

TEST(DataUseTrackerTest, CheckComputeTotalDataUse) {
  TestDataUsePrefService local_state;
  FakeDataUseTracker data_use_tracker(&local_state);
  local_state.ClearDataUsePrefs();
  SetPrefTestValuesOverRatio(&local_state);

  int user_data_use =
      data_use_tracker.ComputeTotalDataUse(prefs::kUserCellDataUse);
  EXPECT_EQ(8 * 100 * 1024, user_data_use);
  int uma_data_use =
      data_use_tracker.ComputeTotalDataUse(prefs::kUmaCellDataUse);
  EXPECT_EQ(4 * 50 * 1024, uma_data_use);
}

TEST(DataUseTrackerTest, CheckShouldUploadLogOnCellular) {
  TestDataUsePrefService local_state;
  FakeDataUseTracker data_use_tracker(&local_state);
  local_state.ClearDataUsePrefs();
  SetPrefTestValuesOverRatio(&local_state);

  bool can_upload = data_use_tracker.ShouldUploadLogOnCellular(50 * 1024);
  EXPECT_TRUE(can_upload);
  can_upload = data_use_tracker.ShouldUploadLogOnCellular(100 * 1024);
  EXPECT_TRUE(can_upload);
  can_upload = data_use_tracker.ShouldUploadLogOnCellular(150 * 1024);
  EXPECT_FALSE(can_upload);

  local_state.ClearDataUsePrefs();
  SetPrefTestValuesValidRatio(&local_state);
  can_upload = data_use_tracker.ShouldUploadLogOnCellular(100 * 1024);
  EXPECT_TRUE(can_upload);
  // this is about 0.49%
  can_upload = data_use_tracker.ShouldUploadLogOnCellular(200 * 1024);
  EXPECT_TRUE(can_upload);
  can_upload = data_use_tracker.ShouldUploadLogOnCellular(300 * 1024);
  EXPECT_FALSE(can_upload);
}

}  // namespace metrics
