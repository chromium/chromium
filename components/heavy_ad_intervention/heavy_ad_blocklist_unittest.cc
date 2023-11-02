// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heavy_ad_intervention/heavy_ad_blocklist.h"

#include <stdint.h>

#include <map>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_delegate.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"
#include "components/heavy_ad_intervention/heavy_ad_features.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace heavy_ad_intervention {

namespace {

// Empty mock class to test the HeavyAdBlocklist.
class EmptyOptOutBlocklistDelegate : public blocklist::OptOutBlocklistDelegate {
 public:
  EmptyOptOutBlocklistDelegate() = default;
};

class TestHeavyAdBlocklist : public HeavyAdBlocklist {
 public:
  TestHeavyAdBlocklist(std::unique_ptr<blocklist::OptOutStore> opt_out_store,
                       base::Clock* clock,
                       blocklist::OptOutBlocklistDelegate* blocklist_delegate)
      : HeavyAdBlocklist(std::move(opt_out_store), clock, blocklist_delegate) {}
  ~TestHeavyAdBlocklist() override = default;

  using HeavyAdBlocklist::GetAllowedTypes;
  using HeavyAdBlocklist::ShouldUseHostPolicy;
  using HeavyAdBlocklist::ShouldUsePersistentPolicy;
  using HeavyAdBlocklist::ShouldUseSessionPolicy;
  using HeavyAdBlocklist::ShouldUseTypePolicy;
};

class HeavyAdBlocklistTest : public testing::Test {
 public:
  HeavyAdBlocklistTest() = default;

  HeavyAdBlocklistTest(const HeavyAdBlocklistTest&) = delete;
  HeavyAdBlocklistTest& operator=(const HeavyAdBlocklistTest&) = delete;

  ~HeavyAdBlocklistTest() override = default;

  void SetUp() override { ConfigBlocklistWithParams({}); }

  void TearDown() override { variations::testing::ClearAllVariationParams(); }

  // Sets up a new blocklist with the given |params|.
  void ConfigBlocklistWithParams(
      const std::map<std::string, std::string>& params) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kHeavyAdPrivacyMitigations, params);
    blocklist_ = std::make_unique<TestHeavyAdBlocklist>(nullptr, &test_clock_,
                                                        &blocklist_delegate_);
  }

  blocklist::BlocklistData::AllowedTypesAndVersions GetAllowedTypes() const {
    return blocklist_->GetAllowedTypes();
  }

 protected:
  base::SimpleTestClock test_clock_;
  std::unique_ptr<TestHeavyAdBlocklist> blocklist_;

 private:
  // Observer to |blocklist_|.
  EmptyOptOutBlocklistDelegate blocklist_delegate_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HeavyAdBlocklistTest, DefaultParams) {
  base::TimeDelta duration;
  size_t history = 0;
  int threshold = 0;
  size_t max_hosts = 0;

  EXPECT_TRUE(blocklist_->ShouldUseHostPolicy(&duration, &history, &threshold,
                                              &max_hosts));
  EXPECT_EQ(base::Days(1), duration);
  EXPECT_EQ(base::Hours(24), duration);
  EXPECT_EQ(5u, history);
  EXPECT_EQ(5, threshold);
  EXPECT_EQ(50u, max_hosts);

  EXPECT_FALSE(blocklist_->ShouldUseTypePolicy(nullptr, nullptr, nullptr));

  blocklist::BlocklistData::AllowedTypesAndVersions types = GetAllowedTypes();
  EXPECT_EQ(1u, types.size());
  const auto iter = types.begin();
  EXPECT_EQ(0, iter->first);
  EXPECT_EQ(0, iter->second);
}

TEST_F(HeavyAdBlocklistTest, HostParams) {
  int host_max_hosts = 11;
  int host_duration_hours = 5;
  int host_threshold = 7;

  ConfigBlocklistWithParams(
      {{"host-duration-hours", base::NumberToString(host_duration_hours)},
       {"host-threshold", base::NumberToString(host_threshold)},
       {"hosts-in-memory", base::NumberToString(host_max_hosts)}});

  base::TimeDelta duration;
  size_t history = 0;
  int threshold = 0;
  size_t max_hosts = 0;

  EXPECT_TRUE(blocklist_->ShouldUseHostPolicy(&duration, &history, &threshold,
                                              &max_hosts));
  EXPECT_EQ(base::Hours(host_duration_hours), duration);
  EXPECT_EQ(host_threshold, static_cast<int>(history));
  EXPECT_EQ(host_threshold, threshold);
  EXPECT_EQ(host_max_hosts, static_cast<int>(max_hosts));
}

TEST_F(HeavyAdBlocklistTest, TypeParams) {
  EXPECT_FALSE(blocklist_->ShouldUseTypePolicy(nullptr, nullptr, nullptr));
}

TEST_F(HeavyAdBlocklistTest, TypeVersionParam) {
  int version = 17;
  ConfigBlocklistWithParams({{"type-version", base::NumberToString(version)}});
  blocklist::BlocklistData::AllowedTypesAndVersions types = GetAllowedTypes();
  EXPECT_EQ(1u, types.size());
  const auto iter = types.begin();
  EXPECT_EQ(0, iter->first);
  EXPECT_EQ(version, iter->second);
}

}  // namespace

}  // namespace heavy_ad_intervention
