// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_black_list.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_delegate.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_item.h"
#include "components/blacklist/opt_out_blacklist/opt_out_store.h"
#include "components/previews/core/previews_experiments.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {

// Mock class to test that PreviewsBlackList notifies the delegate with correct
// events (e.g. New host blacklisted, user blacklisted, and blacklist cleared).
class TestOptOutBlacklistDelegate : public blacklist::OptOutBlacklistDelegate {
 public:
  TestOptOutBlacklistDelegate() {}

  // blacklist::OptOutBlacklistDelegate:
  void OnNewBlacklistedHost(const std::string& host, base::Time time) override {
  }
  void OnUserBlacklistedStatusChange(bool blacklisted) override {}
  void OnBlacklistCleared(base::Time time) override {}
};

class TestPreviewsBlackList : public PreviewsBlackList {
 public:
  TestPreviewsBlackList(
      std::unique_ptr<blacklist::OptOutStore> opt_out_store,
      base::Clock* clock,
      blacklist::OptOutBlacklistDelegate* blacklist_delegate,
      blacklist::BlacklistData::AllowedTypesAndVersions allowed_types)
      : PreviewsBlackList(std::move(opt_out_store),
                          clock,
                          blacklist_delegate,
                          allowed_types) {}
  ~TestPreviewsBlackList() override {}

  bool ShouldUseSessionPolicy(base::TimeDelta* duration,
                              size_t* history,
                              int* threshold) const override {
    return PreviewsBlackList::ShouldUseSessionPolicy(duration, history,
                                                     threshold);
  }
  bool ShouldUsePersistentPolicy(base::TimeDelta* duration,
                                 size_t* history,
                                 int* threshold) const override {
    return PreviewsBlackList::ShouldUsePersistentPolicy(duration, history,
                                                        threshold);
  }
  bool ShouldUseHostPolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold,
                           size_t* max_hosts) const override {
    return PreviewsBlackList::ShouldUseHostPolicy(duration, history, threshold,
                                                  max_hosts);
  }
  bool ShouldUseTypePolicy(base::TimeDelta* duration,
                           size_t* history,
                           int* threshold) const override {
    return PreviewsBlackList::ShouldUseTypePolicy(duration, history, threshold);
  }
};

class PreviewsBlackListTest : public testing::Test {
 public:
  PreviewsBlackListTest() : passed_reasons_({}) {}
  ~PreviewsBlackListTest() override {}

  void TearDown() override { variations::testing::ClearAllVariationParams(); }

  void StartTest() {
    if (params_.size() > 0) {
      ASSERT_TRUE(variations::AssociateVariationParams("ClientSidePreviews",
                                                       "Enabled", params_));
      ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("ClientSidePreviews",
                                                         "Enabled"));
      params_.clear();
    }

    blacklist::BlacklistData::AllowedTypesAndVersions allowed_types;
    allowed_types[static_cast<int>(PreviewsType::OFFLINE)] = 0;
    black_list_ = std::make_unique<TestPreviewsBlackList>(
        nullptr, &test_clock_, &blacklist_delegate_, std::move(allowed_types));

    passed_reasons_ = {};
  }

  void SetHostHistoryParam(size_t host_history) {
    params_["per_host_max_stored_history_length"] =
        base::NumberToString(host_history);
  }

  void SetHostIndifferentHistoryParam(size_t host_indifferent_history) {
    params_["host_indifferent_max_stored_history_length"] =
        base::NumberToString(host_indifferent_history);
  }

  void SetHostThresholdParam(int per_host_threshold) {
    params_["per_host_opt_out_threshold"] =
        base::NumberToString(per_host_threshold);
  }

  void SetHostIndifferentThresholdParam(int host_indifferent_threshold) {
    params_["host_indifferent_opt_out_threshold"] =
        base::NumberToString(host_indifferent_threshold);
  }

  void SetHostDurationParam(int duration_in_days) {
    params_["per_host_black_list_duration_in_days"] =
        base::NumberToString(duration_in_days);
  }

  void SetHostIndifferentDurationParam(int duration_in_days) {
    params_["host_indifferent_black_list_duration_in_days"] =
        base::NumberToString(duration_in_days);
  }

  void SetSingleOptOutDurationParam(int single_opt_out_duration) {
    params_["single_opt_out_duration_in_seconds"] =
        base::NumberToString(single_opt_out_duration);
  }

  void SetMaxHostInBlackListParam(size_t max_hosts_in_blacklist) {
    params_["max_hosts_in_blacklist"] =
        base::NumberToString(max_hosts_in_blacklist);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  // Observer to |black_list_|.
  TestOptOutBlacklistDelegate blacklist_delegate_;

  base::SimpleTestClock test_clock_;
  std::map<std::string, std::string> params_;

  std::unique_ptr<TestPreviewsBlackList> black_list_;
  std::vector<PreviewsEligibilityReason> passed_reasons_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PreviewsBlackListTest);
};

TEST_F(PreviewsBlackListTest, AddPreviewUMA) {
  base::HistogramTester histogram_tester;
  const GURL url("http://www.url.com");

  StartTest();

  black_list_->AddPreviewNavigation(url, false, PreviewsType::OFFLINE);
  histogram_tester.ExpectUniqueSample("Previews.OptOut.UserOptedOut.Offline", 0,
                                      1);
  histogram_tester.ExpectUniqueSample("Previews.OptOut.UserOptedOut", 0, 1);
  black_list_->AddPreviewNavigation(url, true, PreviewsType::OFFLINE);
  histogram_tester.ExpectBucketCount("Previews.OptOut.UserOptedOut.Offline", 1,
                                     1);
  histogram_tester.ExpectBucketCount("Previews.OptOut.UserOptedOut", 1, 1);
}

TEST_F(PreviewsBlackListTest, SessionParams) {
  int duration_seconds = 5;
  SetSingleOptOutDurationParam(duration_seconds);

  StartTest();

  base::TimeDelta duration;
  size_t history = 0;
  int threshold = 0;

  EXPECT_TRUE(
      black_list_->ShouldUseSessionPolicy(&duration, &history, &threshold));
  EXPECT_EQ(base::TimeDelta::FromSeconds(duration_seconds), duration);
  EXPECT_EQ(1u, history);
  EXPECT_EQ(1, threshold);
}

TEST_F(PreviewsBlackListTest, PersistentParams) {
  int duration_days = 5;
  size_t expected_history = 6;
  int expected_threshold = 4;
  SetHostIndifferentThresholdParam(expected_threshold);
  SetHostIndifferentHistoryParam(expected_history);
  SetHostIndifferentDurationParam(duration_days);

  StartTest();

  base::TimeDelta duration;
  size_t history = 0;
  int threshold = 0;

  EXPECT_TRUE(
      black_list_->ShouldUsePersistentPolicy(&duration, &history, &threshold));
  EXPECT_EQ(base::TimeDelta::FromDays(duration_days), duration);
  EXPECT_EQ(expected_history, history);
  EXPECT_EQ(expected_threshold, threshold);
}

TEST_F(PreviewsBlackListTest, HostParams) {
  int duration_days = 5;
  size_t expected_history = 6;
  int expected_threshold = 4;
  size_t expected_max_hosts = 11;
  SetHostThresholdParam(expected_threshold);
  SetHostHistoryParam(expected_history);
  SetHostDurationParam(duration_days);
  SetMaxHostInBlackListParam(expected_max_hosts);

  StartTest();

  base::TimeDelta duration;
  size_t history = 0;
  int threshold = 0;
  size_t max_hosts = 0;

  EXPECT_TRUE(black_list_->ShouldUseHostPolicy(&duration, &history, &threshold,
                                               &max_hosts));
  EXPECT_EQ(base::TimeDelta::FromDays(duration_days), duration);
  EXPECT_EQ(expected_history, history);
  EXPECT_EQ(expected_threshold, threshold);
  EXPECT_EQ(expected_max_hosts, max_hosts);
}

TEST_F(PreviewsBlackListTest, TypeParams) {
  StartTest();
  EXPECT_FALSE(black_list_->ShouldUseTypePolicy(nullptr, nullptr, nullptr));
}

}  // namespace

}  // namespace previews
