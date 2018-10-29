// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_decider_impl.h"

#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_data.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_delegate.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_item.h"
#include "components/blacklist/opt_out_blacklist/opt_out_store.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_logger.h"
#include "components/previews/core/previews_switches.h"
#include "components/variations/variations_associated_data.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {

// A fake default page_id for testing.
const uint64_t kDefaultPageId = 123456;

// This method simulates the actual behavior of the passed in callback, which is
// validated in other tests. For simplicity, offline, lite page, and server LoFi
// use the offline previews check. Client LoFi uses a seperate check to verify
// that types are treated differently.
bool IsPreviewFieldTrialEnabled(PreviewsType type) {
  switch (type) {
    case PreviewsType::OFFLINE:
    case PreviewsType::LITE_PAGE:
      return params::IsOfflinePreviewsEnabled();
    case PreviewsType::DEPRECATED_AMP_REDIRECTION:
      return false;
    case PreviewsType::LOFI:
      return params::IsClientLoFiEnabled();
    case PreviewsType::NOSCRIPT:
      return params::IsNoScriptPreviewsEnabled();
    case PreviewsType::RESOURCE_LOADING_HINTS:
      return params::IsResourceLoadingHintsEnabled();
    case previews::PreviewsType::LITE_PAGE_REDIRECT:
      return params::IsLitePageServerPreviewsEnabled();
    case PreviewsType::NONE:
    case PreviewsType::UNSPECIFIED:
    case PreviewsType::LAST:
      break;
  }
  NOTREACHED();
  return false;
}

// Stub class of PreviewsBlackList to control IsLoadedAndAllowed outcome when
// testing PreviewsDeciderImpl.
class TestPreviewsBlackList : public PreviewsBlackList {
 public:
  TestPreviewsBlackList(PreviewsEligibilityReason status,
                        blacklist::OptOutBlacklistDelegate* blacklist_delegate)
      : PreviewsBlackList(nullptr,
                          base::DefaultClock::GetInstance(),
                          blacklist_delegate,
                          {}),
        status_(status) {}
  ~TestPreviewsBlackList() override {}

  // PreviewsBlackList:
  PreviewsEligibilityReason IsLoadedAndAllowed(
      const GURL& url,
      PreviewsType type,
      bool ignore_long_term_black_list_rules,
      std::vector<PreviewsEligibilityReason>* passed_reasons) const override {
    std::vector<PreviewsEligibilityReason> ordered_reasons = {
        PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
        PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT};

    if (!ignore_long_term_black_list_rules) {
      ordered_reasons.push_back(PreviewsEligibilityReason::USER_BLACKLISTED);
      ordered_reasons.push_back(PreviewsEligibilityReason::HOST_BLACKLISTED);
    }

    for (auto reason : ordered_reasons) {
      if (status_ == reason) {
        return status_;
      }
      passed_reasons->push_back(reason);
    }

    return PreviewsEligibilityReason::ALLOWED;
  }

 private:
  PreviewsEligibilityReason status_;
};

// Stub class of PreviewsOptimizationGuide to control IsWhitelisted and
// IsBlacklisted outcomes when testing PreviewsDeciderImpl.
class TestPreviewsOptimizationGuide : public PreviewsOptimizationGuide {
 public:
  TestPreviewsOptimizationGuide(
      optimization_guide::OptimizationGuideService* optimization_guide_service,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
      : PreviewsOptimizationGuide(optimization_guide_service, io_task_runner) {}
  ~TestPreviewsOptimizationGuide() override {}

  // PreviewsOptimizationGuide:
  bool IsWhitelisted(PreviewsUserData* previews_user_data,
                     const GURL& url,

                     PreviewsType type) const override {
    EXPECT_TRUE(type == PreviewsType::NOSCRIPT ||
                type == PreviewsType::RESOURCE_LOADING_HINTS);
    if (type == PreviewsType::NOSCRIPT) {
      return url.host().compare("whitelisted.example.com") == 0 ||
             url.host().compare("noscript_only_whitelisted.example.com") == 0;
    }
    if (type == PreviewsType::RESOURCE_LOADING_HINTS) {
      return url.host().compare("whitelisted.example.com") == 0;
    }
    return false;
  }

  // PreviewsOptimizationGuide:
  bool IsBlacklisted(const GURL& url, PreviewsType type) const override {
    EXPECT_TRUE(type == PreviewsType::LITE_PAGE_REDIRECT);
    if (type == PreviewsType::LITE_PAGE_REDIRECT) {
      return url.host().compare("blacklisted.example.com") == 0;
    }
    return false;
  }
};

// Stub class of PreviewsUIService to test logging functionalities in
// PreviewsDeciderImpl.
class TestPreviewsUIService : public PreviewsUIService {
 public:
  TestPreviewsUIService(
      std::unique_ptr<PreviewsDeciderImpl> previews_decider_impl,
      std::unique_ptr<blacklist::OptOutStore> previews_opt_out_store,
      std::unique_ptr<PreviewsOptimizationGuide> previews_opt_guide,
      const PreviewsIsEnabledCallback& is_enabled_callback,
      std::unique_ptr<PreviewsLogger> logger,
      blacklist::BlacklistData::AllowedTypesAndVersions allowed_types,
      network::NetworkQualityTracker* network_quality_tracker)
      : PreviewsUIService(std::move(previews_decider_impl),
                          std::move(previews_opt_out_store),
                          std::move(previews_opt_guide),
                          is_enabled_callback,
                          std::move(logger),
                          std::move(allowed_types),
                          network_quality_tracker),
        user_blacklisted_(false),
        blacklist_ignored_(false) {}

  // PreviewsUIService:
  void OnNewBlacklistedHost(const std::string& host, base::Time time) override {
    host_blacklisted_ = host;
    host_blacklisted_time_ = time;
  }
  void OnUserBlacklistedStatusChange(bool blacklisted) override {
    user_blacklisted_ = blacklisted;
  }
  void OnBlacklistCleared(base::Time time) override {
    blacklist_cleared_time_ = time;
  }
  void OnIgnoreBlacklistDecisionStatusChanged(bool ignored) override {
    blacklist_ignored_ = ignored;
  }

  // Expose passed in LogPreviewDecision parameters.
  const std::vector<PreviewsEligibilityReason>& decision_reasons() const {
    return decision_reasons_;
  }
  const std::vector<GURL>& decision_urls() const { return decision_urls_; }
  const std::vector<PreviewsType>& decision_types() const {
    return decision_types_;
  }
  const std::vector<base::Time>& decision_times() const {
    return decision_times_;
  }
  const std::vector<std::vector<PreviewsEligibilityReason>>&
  decision_passed_reasons() const {
    return decision_passed_reasons_;
  }
  const std::vector<uint64_t>& decision_ids() const { return decision_ids_; }

  // Expose passed in LogPreviewsNavigation parameters.
  const std::vector<GURL>& navigation_urls() const { return navigation_urls_; }
  const std::vector<bool>& navigation_opt_outs() const {
    return navigation_opt_outs_;
  }
  const std::vector<base::Time>& navigation_times() const {
    return navigation_times_;
  }
  const std::vector<PreviewsType>& navigation_types() const {
    return navigation_types_;
  }
  const std::vector<uint64_t>& navigation_page_ids() const {
    return navigation_page_ids_;
  }

  // Expose passed in params for hosts and user blacklist event.
  std::string host_blacklisted() const { return host_blacklisted_; }
  base::Time host_blacklisted_time() const { return host_blacklisted_time_; }
  bool user_blacklisted() const { return user_blacklisted_; }
  base::Time blacklist_cleared_time() const { return blacklist_cleared_time_; }

  // Expose the status of blacklist decisions ignored.
  bool blacklist_ignored() const { return blacklist_ignored_; }

 private:
  // PreviewsUIService:
  void LogPreviewNavigation(const GURL& url,
                            PreviewsType type,
                            bool opt_out,
                            base::Time time,
                            uint64_t page_id) override {
    navigation_urls_.push_back(url);
    navigation_opt_outs_.push_back(opt_out);
    navigation_types_.push_back(type);
    navigation_times_.push_back(time);
    navigation_page_ids_.push_back(page_id);
  }

  void LogPreviewDecisionMade(
      PreviewsEligibilityReason reason,
      const GURL& url,
      base::Time time,
      PreviewsType type,
      std::vector<PreviewsEligibilityReason>&& passed_reasons,
      uint64_t page_id) override {
    decision_reasons_.push_back(reason);
    decision_urls_.push_back(GURL(url));
    decision_times_.push_back(time);
    decision_types_.push_back(type);
    decision_passed_reasons_.push_back(std::move(passed_reasons));
    decision_ids_.push_back(page_id);
  }

  // Passed in params for blacklist status events.
  std::string host_blacklisted_;
  base::Time host_blacklisted_time_;
  bool user_blacklisted_;
  base::Time blacklist_cleared_time_;

  // Passed in LogPreviewDecision parameters.
  std::vector<PreviewsEligibilityReason> decision_reasons_;
  std::vector<GURL> decision_urls_;
  std::vector<PreviewsType> decision_types_;
  std::vector<base::Time> decision_times_;
  std::vector<std::vector<PreviewsEligibilityReason>> decision_passed_reasons_;
  std::vector<uint64_t> decision_ids_;

  // Passed in LogPreviewsNavigation parameters.
  std::vector<GURL> navigation_urls_;
  std::vector<bool> navigation_opt_outs_;
  std::vector<base::Time> navigation_times_;
  std::vector<PreviewsType> navigation_types_;
  std::vector<uint64_t> navigation_page_ids_;

  // Whether the blacklist decisions are ignored or not.
  bool blacklist_ignored_;
};

class TestPreviewsDeciderImpl : public PreviewsDeciderImpl {
 public:
  TestPreviewsDeciderImpl(base::Clock* clock) : PreviewsDeciderImpl(clock) {}
  ~TestPreviewsDeciderImpl() override {}

  // Expose the injecting blacklist method from PreviewsDeciderImpl, and inject
  // |blacklist| into |this|.
  void InjectTestBlacklist(std::unique_ptr<PreviewsBlackList> blacklist) {
    SetPreviewsBlacklistForTesting(std::move(blacklist));
  }
};

void RunLoadCallback(blacklist::LoadBlackListCallback callback,
                     std::unique_ptr<blacklist::BlacklistData> data) {
  std::move(callback).Run(std::move(data));
}

class TestOptOutStore : public blacklist::OptOutStore {
 public:
  TestOptOutStore() {}
  ~TestOptOutStore() override {}

 private:
  // blacklist::OptOutStore implementation:
  void AddEntry(bool opt_out,
                const std::string& host_name,
                int type,
                base::Time now) override {}

  void LoadBlackList(std::unique_ptr<blacklist::BlacklistData> data,
                     blacklist::LoadBlackListCallback callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&RunLoadCallback, std::move(callback), std::move(data)));
  }

  void ClearBlackList(base::Time begin_time, base::Time end_time) override {}
};

class PreviewsDeciderImplTest : public testing::Test {
 public:
  PreviewsDeciderImplTest()
      : field_trial_list_(nullptr),
        previews_decider_impl_(nullptr),
        optimization_guide_service_(
            scoped_task_environment_.GetMainThreadTaskRunner()) {
    clock_.SetNow(base::Time::Now());

    network_quality_tracker_.ReportEffectiveConnectionTypeForTesting(
        net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  }

  ~PreviewsDeciderImplTest() override {
    // TODO(dougarnett) bug 781975: Consider switching to Feature API and
    // ScopedFeatureList (and dropping components/variations dep).
    variations::testing::ClearAllVariationParams();
  }

  void TearDown() override { ui_service_.reset(); }

  void InitializeUIServiceWithoutWaitingForBlackList() {
    blacklist::BlacklistData::AllowedTypesAndVersions allowed_types;
    allowed_types[static_cast<int>(PreviewsType::OFFLINE)] = 0;
    allowed_types[static_cast<int>(PreviewsType::LOFI)] = 0;
    allowed_types[static_cast<int>(PreviewsType::LITE_PAGE)] = 0;
    allowed_types[static_cast<int>(PreviewsType::LITE_PAGE_REDIRECT)] = 0;
    allowed_types[static_cast<int>(PreviewsType::NOSCRIPT)] = 0;
    allowed_types[static_cast<int>(PreviewsType::RESOURCE_LOADING_HINTS)] = 0;

    std::unique_ptr<TestPreviewsDeciderImpl> previews_decider_impl =
        std::make_unique<TestPreviewsDeciderImpl>(&clock_);
    previews_decider_impl_ = previews_decider_impl.get();
    ui_service_.reset(new TestPreviewsUIService(
        std::move(previews_decider_impl), std::make_unique<TestOptOutStore>(),
        std::make_unique<TestPreviewsOptimizationGuide>(
            &optimization_guide_service_,
            scoped_task_environment_.GetMainThreadTaskRunner()),
        base::BindRepeating(&IsPreviewFieldTrialEnabled),
        std::make_unique<PreviewsLogger>(), std::move(allowed_types),
        &network_quality_tracker_));
  }

  void InitializeUIService() {
    InitializeUIServiceWithoutWaitingForBlackList();
    scoped_task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  TestPreviewsDeciderImpl* previews_decider_impl() {
    return previews_decider_impl_;
  }
  TestPreviewsUIService* ui_service() { return ui_service_.get(); }

  void ReportEffectiveConnectionType(
      net::EffectiveConnectionType effective_connection_type) {
    network_quality_tracker_.ReportEffectiveConnectionTypeForTesting(
        effective_connection_type);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::SimpleTestClock clock_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::FieldTrialList field_trial_list_;
  TestPreviewsDeciderImpl* previews_decider_impl_;
  optimization_guide::OptimizationGuideService optimization_guide_service_;
  std::unique_ptr<TestPreviewsUIService> ui_service_;
  network::TestNetworkQualityTracker network_quality_tracker_;
};

TEST_F(PreviewsDeciderImplTest, AllPreviewsDisabledByFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kClientLoFi, features::kNoScriptPreviews},
      {features::kPreviews} /* disable_features */);
  InitializeUIService();

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  PreviewsUserData user_data(kDefaultPageId);
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false, PreviewsType::LOFI,
      previews::params::GetECTThresholdForPreview(
          previews::PreviewsType::NOSCRIPT),
      std::vector<std::string>(), false));

  PreviewsUserData user_data2(kDefaultPageId);
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data2, GURL("https://www.google.com"), false,
      PreviewsType::NOSCRIPT,
      previews::params::GetECTThresholdForPreview(
          previews::PreviewsType::NOSCRIPT),
      std::vector<std::string>(), false));
}

// Tests most of the reasons that a preview could be disallowed because of the
// state of the blacklist. Excluded values are USER_RECENTLY_OPTED_OUT,
// USER_BLACKLISTED, HOST_BLACKLISTED. These are internal to the blacklist.
TEST_F(PreviewsDeciderImplTest, TestDisallowPreviewBecauseOfBlackListState) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPreviews);
  base::HistogramTester histogram_tester;

  PreviewsUserData user_data(kDefaultPageId);

  InitializeUIServiceWithoutWaitingForBlackList();

  // The blacklist is not loaded yet.
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreview(
      &user_data, GURL("https://www.google.com"), false,
      PreviewsType::OFFLINE));
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED),
      1);

  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount("Previews.EligibilityReason.Offline", 1);

  // Return one of the failing statuses from the blacklist; cause the blacklist
  // to not be loaded by clearing the blacklist.
  base::Time now = base::Time::Now();
  previews_decider_impl()->ClearBlackList(now, now);

  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreview(
      &user_data, GURL("https://www.google.com"), false,
      PreviewsType::OFFLINE));
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED),
      2);
  histogram_tester.ExpectTotalCount("Previews.EligibilityReason.NoScript", 0);

  variations::testing::ClearAllVariationParams();
}

TEST_F(PreviewsDeciderImplTest, TestSetBlacklistBoolDueToBlackListState) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPreviews);

  PreviewsUserData user_data(kDefaultPageId);

  base::HistogramTester histogram_tester;
  InitializeUIServiceWithoutWaitingForBlackList();
  base::RunLoop().RunUntilIdle();
  previews_decider_impl()->AddPreviewNavigation(
      GURL("https://www.google.com"), true, PreviewsType::LITE_PAGE, 1);

  EXPECT_FALSE(user_data.black_listed_for_lite_page());
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false,
      PreviewsType::LITE_PAGE, net::EFFECTIVE_CONNECTION_TYPE_2G, {}, false));
  EXPECT_TRUE(user_data.black_listed_for_lite_page());
}

TEST_F(PreviewsDeciderImplTest,
       TestDisallowOfflineWhenNetworkQualityUnavailable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPreviews);
  InitializeUIService();

  PreviewsUserData user_data(kDefaultPageId);

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreview(
      &user_data, GURL("https://www.google.com"), false,
      PreviewsType::OFFLINE));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE),
      1);
}

TEST_F(PreviewsDeciderImplTest, TestAllowLitePageWhenNetworkQualityFast) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPreviews);
  InitializeUIService();

  PreviewsUserData user_data(kDefaultPageId);
  // LoFi and LitePage check NQE on their own.
  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_3G);

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false,
      PreviewsType::LITE_PAGE, net::EFFECTIVE_CONNECTION_TYPE_4G,
      std::vector<std::string>(), false));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.LitePage",
      static_cast<int>(PreviewsEligibilityReason::ALLOWED), 1);
}

TEST_F(PreviewsDeciderImplTest, TestDisallowOfflineWhenNetworkQualityFast) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPreviews);
  InitializeUIService();

  PreviewsUserData user_data(kDefaultPageId);

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_3G);
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreview(
      &user_data, GURL("https://www.google.com"), false,
      PreviewsType::OFFLINE));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::NETWORK_NOT_SLOW), 1);
}

TEST_F(PreviewsDeciderImplTest, TestDisallowOfflineOnReload) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPreviews);
  InitializeUIService();

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  PreviewsUserData user_data(kDefaultPageId);

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreview(
      &user_data, GURL("https://www.google.com"), true, PreviewsType::OFFLINE));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.Offline",
      static_cast<int>(PreviewsEligibilityReason::RELOAD_DISALLOWED), 1);
}

TEST_F(PreviewsDeciderImplTest, TestAllowOffline) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPreviews);
  InitializeUIService();

  const struct {
    net::EffectiveConnectionType effective_connection_type;
    bool expected_offline_allowed;
  } tests[] = {
      {net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN, false},
      {net::EFFECTIVE_CONNECTION_TYPE_OFFLINE, false},
      {net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G, true},
      {net::EFFECTIVE_CONNECTION_TYPE_2G, true},
      {net::EFFECTIVE_CONNECTION_TYPE_3G, false},
  };
  for (const auto& test : tests) {
    ReportEffectiveConnectionType(test.effective_connection_type);

    PreviewsUserData user_data(kDefaultPageId);
    base::HistogramTester histogram_tester;
    EXPECT_EQ(test.expected_offline_allowed,
              previews_decider_impl()->ShouldAllowPreview(
                  &user_data, GURL("https://www.google.com"), false,
                  PreviewsType::OFFLINE))
        << " effective_connection_type=" << test.effective_connection_type;
    if (test.expected_offline_allowed) {
      histogram_tester.ExpectUniqueSample(
          "Previews.EligibilityReason.Offline",
          static_cast<int>(PreviewsEligibilityReason::ALLOWED), 1);
    } else {
      histogram_tester.ExpectBucketCount(
          "Previews.EligibilityReason.Offline",
          static_cast<int>(PreviewsEligibilityReason::ALLOWED), 0);
    }
  }
}

TEST_F(PreviewsDeciderImplTest, ClientLoFiDisallowedWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kPreviews},
                                       {features::kClientLoFi});
  InitializeUIService();

  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            params::EffectiveConnectionTypeThresholdForClientLoFi());
  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  PreviewsUserData user_data(kDefaultPageId);
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false, PreviewsType::LOFI,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial(), false));
  histogram_tester.ExpectTotalCount("Previews.EligibilityReason.LoFi", 0);
}

TEST_F(PreviewsDeciderImplTest,
       ClientLoFiDisallowedWhenNetworkQualityUnavailable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kClientLoFi}, {});
  InitializeUIService();

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);

  PreviewsUserData user_data(kDefaultPageId);
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false, PreviewsType::LOFI,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial(), false));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.LoFi",
      static_cast<int>(PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE),
      1);
}

TEST_F(PreviewsDeciderImplTest, ClientLoFiDisallowedWhenNetworkFast) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kClientLoFi}, {});
  InitializeUIService();

  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            params::EffectiveConnectionTypeThresholdForClientLoFi());
  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_3G);

  base::HistogramTester histogram_tester;
  PreviewsUserData user_data(kDefaultPageId);
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false, PreviewsType::LOFI,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial(), false));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.LoFi",
      static_cast<int>(PreviewsEligibilityReason::NETWORK_NOT_SLOW), 1);
}

TEST_F(PreviewsDeciderImplTest, ClientLoFiAllowed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kClientLoFi}, {});
  InitializeUIService();

  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            params::EffectiveConnectionTypeThresholdForClientLoFi());

  const struct {
    net::EffectiveConnectionType effective_connection_type;
    bool expected_client_lofi_allowed;
  } tests[] = {
      {net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN, false},
      {net::EFFECTIVE_CONNECTION_TYPE_OFFLINE, false},
      {net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G, true},
      {net::EFFECTIVE_CONNECTION_TYPE_2G, true},
      {net::EFFECTIVE_CONNECTION_TYPE_3G, false},
  };

  for (const auto& test : tests) {
    ReportEffectiveConnectionType(test.effective_connection_type);

    base::HistogramTester histogram_tester;
    PreviewsUserData user_data(kDefaultPageId);
    EXPECT_EQ(test.expected_client_lofi_allowed,
              previews_decider_impl()->ShouldAllowPreviewAtECT(
                  &user_data, GURL("https://www.google.com"), false,
                  PreviewsType::LOFI,
                  params::EffectiveConnectionTypeThresholdForClientLoFi(),
                  params::GetBlackListedHostsForClientLoFiFieldTrial(), false))
        << " effective_connection_type=" << test.effective_connection_type;
    if (test.expected_client_lofi_allowed) {
      histogram_tester.ExpectUniqueSample(
          "Previews.EligibilityReason.LoFi",
          static_cast<int>(PreviewsEligibilityReason::ALLOWED), 1);
    } else {
      histogram_tester.ExpectBucketCount(
          "Previews.EligibilityReason.LoFi",
          static_cast<int>(PreviewsEligibilityReason::ALLOWED), 0);
    }
  }
}

TEST_F(PreviewsDeciderImplTest, MissingHostDisallowed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kClientLoFi}, {});
  InitializeUIService();

  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            params::EffectiveConnectionTypeThresholdForClientLoFi());
  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  PreviewsUserData user_data(kDefaultPageId);
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("file:///sdcard"), false, PreviewsType::LOFI,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial(), false));
}

TEST_F(PreviewsDeciderImplTest, ClientLoFiAllowedOnReload) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kClientLoFi}, {});
  InitializeUIService();

  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            params::EffectiveConnectionTypeThresholdForClientLoFi());
  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  PreviewsUserData user_data(kDefaultPageId);

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), true, PreviewsType::LOFI,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial(), false));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.LoFi",
      static_cast<int>(PreviewsEligibilityReason::ALLOWED), 1);
}

TEST_F(PreviewsDeciderImplTest, ClientLoFiObeysHostBlackListFromServer) {
  base::test::ScopedFeatureList scoped_previews_feature_list;
  scoped_previews_feature_list.InitAndEnableFeature(features::kPreviews);

  // Use a nested ScopedFeatureList so that parameters can be set.
  base::test::ScopedFeatureList scoped_lofi_feature_list;
  scoped_lofi_feature_list.InitAndEnableFeatureWithParameters(
      features::kClientLoFi, {{"max_allowed_effective_connection_type", "2G"},
                              {"short_host_blacklist", "foo.com, ,bar.net "}});

  InitializeUIService();

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  const struct {
    const char* url;
    bool expected_client_lofi_allowed;
  } tests[] = {
      {"http://example.com", true},      {"http://foo.com", false},
      {"https://foo.com", false},        {"http://www.foo.com", true},
      {"http://m.foo.com", true},        {"http://foo.net", true},
      {"http://foo.com/example", false}, {"http://bar.net", false},
      {"http://bar.net.tld", true},
  };

  for (const auto& test : tests) {
    base::HistogramTester histogram_tester;

    PreviewsUserData user_data(kDefaultPageId);

    EXPECT_EQ(test.expected_client_lofi_allowed,
              previews_decider_impl()->ShouldAllowPreviewAtECT(
                  &user_data, GURL(test.url), false, PreviewsType::LOFI,
                  params::EffectiveConnectionTypeThresholdForClientLoFi(),
                  params::GetBlackListedHostsForClientLoFiFieldTrial(), false));

    histogram_tester.ExpectUniqueSample(
        "Previews.EligibilityReason.LoFi",
        static_cast<int>(
            test.expected_client_lofi_allowed
                ? PreviewsEligibilityReason::ALLOWED
                : PreviewsEligibilityReason::HOST_BLACKLISTED_BY_SERVER),
        1);
  }
}

TEST_F(PreviewsDeciderImplTest, NoScriptFeatureDefaultBehavior) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPreviews);
  InitializeUIService();

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  base::HistogramTester histogram_tester;
  PreviewsUserData user_data(kDefaultPageId);
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false, PreviewsType::NOSCRIPT,
      previews::params::GetECTThresholdForPreview(
          previews::PreviewsType::NOSCRIPT),
      std::vector<std::string>(), false));
#if defined(OS_ANDROID)
  // Enabled by default on Android but no server whitelist.
  histogram_tester.ExpectTotalCount("Previews.EligibilityReason.NoScript", 1);
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.NoScript",
      static_cast<int>(
          PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER),
      1);
#else   // !defined(OS_ANDROID)
  // Disabled by default on non-Android.
  histogram_tester.ExpectTotalCount("Previews.EligibilityReason.NoScript", 0);
#endif  // defined(OS_ANDROID)
}

TEST_F(PreviewsDeciderImplTest, NoScriptAllowedByFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kNoScriptPreviews},
      {features::kOptimizationHints});
  InitializeUIService();

  const struct {
    net::EffectiveConnectionType effective_connection_type;
    bool expected_noscript_allowed;
  } tests[] = {
      {net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN, false},
      {net::EFFECTIVE_CONNECTION_TYPE_OFFLINE, false},
      {net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G, true},
      {net::EFFECTIVE_CONNECTION_TYPE_2G, true},
      {net::EFFECTIVE_CONNECTION_TYPE_3G, false},
  };

  for (const auto& test : tests) {
    ReportEffectiveConnectionType(test.effective_connection_type);

    base::HistogramTester histogram_tester;
    PreviewsUserData user_data(kDefaultPageId);
    EXPECT_EQ(test.expected_noscript_allowed,
              previews_decider_impl()->ShouldAllowPreviewAtECT(
                  &user_data, GURL("https://www.google.com"), false,
                  PreviewsType::NOSCRIPT,
                  previews::params::GetECTThresholdForPreview(
                      previews::PreviewsType::NOSCRIPT),
                  std::vector<std::string>(), false))
        << " effective_connection_type=" << test.effective_connection_type;
    if (test.expected_noscript_allowed) {
      histogram_tester.ExpectUniqueSample(
          "Previews.EligibilityReason.NoScript",
          static_cast<int>(
              PreviewsEligibilityReason::ALLOWED_WITHOUT_OPTIMIZATION_HINTS),
          1);
    } else {
      histogram_tester.ExpectBucketCount(
          "Previews.EligibilityReason.NoScript",
          static_cast<int>(
              PreviewsEligibilityReason::ALLOWED_WITHOUT_OPTIMIZATION_HINTS),
          0);
    }
  }
}

TEST_F(PreviewsDeciderImplTest, NoScriptAllowedByFeatureWithWhitelist) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kNoScriptPreviews,
       features::kOptimizationHints},
      {});
  InitializeUIService();

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  base::HistogramTester histogram_tester;

  PreviewsUserData user_data(kDefaultPageId);
  // First verify no preview for non-whitelisted url.
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false, PreviewsType::NOSCRIPT,
      previews::params::GetECTThresholdForPreview(
          previews::PreviewsType::NOSCRIPT),
      std::vector<std::string>(), false));

  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.NoScript",
      static_cast<int>(
          PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER),
      1);

  // Now verify preview for whitelisted url.
  EXPECT_TRUE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://whitelisted.example.com"), false,
      PreviewsType::NOSCRIPT,
      previews::params::GetECTThresholdForPreview(
          previews::PreviewsType::NOSCRIPT),
      std::vector<std::string>(), false));

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.NoScript",
      static_cast<int>(PreviewsEligibilityReason::ALLOWED), 1);
}

TEST_F(PreviewsDeciderImplTest, NoScriptCommitTimeWhitelistCheck) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kNoScriptPreviews,
       features::kOptimizationHints},
      {});
  InitializeUIService();

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  // First verify not allowed for non-whitelisted url.
  {
    base::HistogramTester histogram_tester;
    PreviewsUserData user_data(kDefaultPageId);
    EXPECT_FALSE(previews_decider_impl()->IsURLAllowedForPreview(
        &user_data, GURL("https://www.google.com"), PreviewsType::NOSCRIPT));

    histogram_tester.ExpectUniqueSample(
        "Previews.EligibilityReason.NoScript",
        static_cast<int>(
            PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER),
        1);
  }

  // Now verify preview for whitelisted url.
  {
    base::HistogramTester histogram_tester;
    PreviewsUserData user_data(kDefaultPageId);
    EXPECT_TRUE(previews_decider_impl()->IsURLAllowedForPreview(
        &user_data, GURL("https://whitelisted.example.com"),
        PreviewsType::NOSCRIPT));

    // Expect no eligibility logging.
    histogram_tester.ExpectTotalCount("Previews.EligibilityReason.NoScript", 0);
  }
}

TEST_F(PreviewsDeciderImplTest,
       LitePageRedirectAllowedWithoutOptimizationHints) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kLitePageServerPreviews},
      {features::kOptimizationHints});
  InitializeUIService();

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  base::HistogramTester histogram_tester;
  PreviewsUserData user_data(kDefaultPageId);
  EXPECT_TRUE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false,
      PreviewsType::LITE_PAGE_REDIRECT,
      previews::params::GetECTThresholdForPreview(
          previews::PreviewsType::LITE_PAGE_REDIRECT),
      std::vector<std::string>(), false));

  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.LitePageRedirect",
      static_cast<int>(
          PreviewsEligibilityReason::ALLOWED_WITHOUT_OPTIMIZATION_HINTS),
      1);
}

TEST_F(PreviewsDeciderImplTest, LitePageRedirectDisallowedByServerBlacklist) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kLitePageServerPreviews,
       features::kOptimizationHints},
      {});
  InitializeUIService();

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  base::HistogramTester histogram_tester;

  PreviewsUserData user_data(kDefaultPageId);
  // First verify preview allowed for non-whitelisted url.
  EXPECT_TRUE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false,
      PreviewsType::LITE_PAGE_REDIRECT,
      previews::params::GetECTThresholdForPreview(
          previews::PreviewsType::LITE_PAGE_REDIRECT),
      std::vector<std::string>(), false));

  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.LitePageRedirect",
      static_cast<int>(PreviewsEligibilityReason::ALLOWED), 1);

  // Now verify no preview for blacklisted url.
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://blacklisted.example.com"), false,
      PreviewsType::LITE_PAGE_REDIRECT,
      previews::params::GetECTThresholdForPreview(
          previews::PreviewsType::LITE_PAGE_REDIRECT),
      std::vector<std::string>(), false));

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.LitePageRedirect",
      static_cast<int>(PreviewsEligibilityReason::HOST_BLACKLISTED_BY_SERVER),
      1);
}

TEST_F(PreviewsDeciderImplTest, ResourceLoadingHintsDisallowedByDefault) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kOptimizationHints}, {});
  InitializeUIService();

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  base::HistogramTester histogram_tester;
  PreviewsUserData user_data(kDefaultPageId);
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false,
      PreviewsType::RESOURCE_LOADING_HINTS,
      previews::params::GetECTThresholdForPreview(
          previews::PreviewsType::RESOURCE_LOADING_HINTS),
      std::vector<std::string>(), false));
}

TEST_F(PreviewsDeciderImplTest,
       ResourceLoadingHintsDisallowedWithoutOptimizationHints) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kResourceLoadingHints},
      {features::kOptimizationHints});
  InitializeUIService();

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  base::HistogramTester histogram_tester;
  PreviewsUserData user_data(kDefaultPageId);
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://whitelisted.example.com"), false,
      PreviewsType::RESOURCE_LOADING_HINTS,
      previews::params::GetECTThresholdForPreview(
          previews::PreviewsType::RESOURCE_LOADING_HINTS),
      std::vector<std::string>(), false));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(
          PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER),
      1);
}

TEST_F(PreviewsDeciderImplTest, ResourceLoadingHintsAllowedByFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kResourceLoadingHints,
       features::kOptimizationHints},
      {});
  InitializeUIService();

  const struct {
    net::EffectiveConnectionType effective_connection_type;
    bool expected_resource_loading_hints_allowed;
  } tests[] = {
      {net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN, false},
      {net::EFFECTIVE_CONNECTION_TYPE_OFFLINE, false},
      {net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G, true},
      {net::EFFECTIVE_CONNECTION_TYPE_2G, true},
      {net::EFFECTIVE_CONNECTION_TYPE_3G, false},
  };

  for (const auto& test : tests) {
    ReportEffectiveConnectionType(test.effective_connection_type);

    base::HistogramTester histogram_tester;
    PreviewsUserData user_data(kDefaultPageId);

    // Check whitelisted URL.
    EXPECT_EQ(test.expected_resource_loading_hints_allowed,
              previews_decider_impl()->ShouldAllowPreviewAtECT(
                  &user_data, GURL("https://whitelisted.example.com"), false,
                  PreviewsType::RESOURCE_LOADING_HINTS,
                  previews::params::GetECTThresholdForPreview(
                      previews::PreviewsType::RESOURCE_LOADING_HINTS),
                  std::vector<std::string>(), false))
        << " effective_connection_type=" << test.effective_connection_type;
    if (test.expected_resource_loading_hints_allowed) {
      histogram_tester.ExpectUniqueSample(
          "Previews.EligibilityReason.ResourceLoadingHints",
          static_cast<int>(PreviewsEligibilityReason::ALLOWED), 1);
    } else if (test.effective_connection_type ==
               net::EFFECTIVE_CONNECTION_TYPE_3G) {
      histogram_tester.ExpectBucketCount(
          "Previews.EligibilityReason.ResourceLoadingHints",
          static_cast<int>(PreviewsEligibilityReason::NETWORK_NOT_SLOW), 1);
    } else {
      histogram_tester.ExpectBucketCount(
          "Previews.EligibilityReason.ResourceLoadingHints",
          static_cast<int>(
              PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE),
          1);
    }
  }
}

TEST_F(PreviewsDeciderImplTest,
       ResourceLoadingHintsAllowedByFeatureWithoutKnownHints) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kResourceLoadingHints,
       features::kOptimizationHints},
      {});
  InitializeUIService();

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  base::HistogramTester histogram_tester;

  PreviewsUserData user_data(kDefaultPageId);
  // First verify preview allowed for url without known hints.
  EXPECT_TRUE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false,
      PreviewsType::RESOURCE_LOADING_HINTS,
      previews::params::GetECTThresholdForPreview(
          previews::PreviewsType::RESOURCE_LOADING_HINTS),
      std::vector<std::string>(), false));

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.ResourceLoadingHints",
      static_cast<int>(PreviewsEligibilityReason::ALLOWED), 1);
}

TEST_F(PreviewsDeciderImplTest, ResourceLoadingHintsCommitTimeWhitelistCheck) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kResourceLoadingHints,
       features::kOptimizationHints},
      {});
  InitializeUIService();

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  // First verify not allowed for non-whitelisted url.
  {
    base::HistogramTester histogram_tester;
    PreviewsUserData user_data(kDefaultPageId);
    EXPECT_FALSE(previews_decider_impl()->IsURLAllowedForPreview(
        &user_data, GURL("https://www.google.com"),
        PreviewsType::RESOURCE_LOADING_HINTS));

    histogram_tester.ExpectUniqueSample(
        "Previews.EligibilityReason.ResourceLoadingHints",
        static_cast<int>(
            PreviewsEligibilityReason::HOST_NOT_WHITELISTED_BY_SERVER),
        1);
  }

  // Now verify preview for whitelisted url.
  {
    base::HistogramTester histogram_tester;
    PreviewsUserData user_data(kDefaultPageId);
    EXPECT_TRUE(previews_decider_impl()->IsURLAllowedForPreview(
        &user_data, GURL("https://whitelisted.example.com"),
        PreviewsType::RESOURCE_LOADING_HINTS));

    // Expect no eligibility logging.
    histogram_tester.ExpectTotalCount(
        "Previews.EligibilityReason.ResourceLoadingHints", 0);
  }
}

TEST_F(PreviewsDeciderImplTest, LogPreviewNavigationPassInCorrectParams) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPreviews);
  InitializeUIService();
  const GURL url("http://www.url_a.com/url_a");
  const bool opt_out = true;
  const PreviewsType type = PreviewsType::OFFLINE;
  const base::Time time = base::Time::Now();
  const uint64_t page_id = 1234;

  previews_decider_impl()->LogPreviewNavigation(url, opt_out, type, time,
                                                page_id);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(ui_service()->navigation_urls(), ::testing::ElementsAre(url));
  EXPECT_THAT(ui_service()->navigation_opt_outs(),
              ::testing::ElementsAre(opt_out));
  EXPECT_THAT(ui_service()->navigation_types(), ::testing::ElementsAre(type));
  EXPECT_THAT(ui_service()->navigation_times(), ::testing::ElementsAre(time));
  EXPECT_THAT(ui_service()->navigation_page_ids(),
              ::testing::ElementsAre(page_id));
}

TEST_F(PreviewsDeciderImplTest, LogPreviewDecisionMadePassInCorrectParams) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPreviews);
  InitializeUIService();
  const PreviewsEligibilityReason reason(
      PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE);
  const GURL url("http://www.url_a.com/url_a");
  const base::Time time = base::Time::Now();
  const PreviewsType type = PreviewsType::OFFLINE;
  std::vector<PreviewsEligibilityReason> passed_reasons = {
      PreviewsEligibilityReason::NETWORK_NOT_SLOW,
      PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
      PreviewsEligibilityReason::RELOAD_DISALLOWED,
  };
  const std::vector<PreviewsEligibilityReason> expected_passed_reasons(
      passed_reasons);
  const uint64_t page_id = 1234;

  previews_decider_impl()->LogPreviewDecisionMade(
      reason, url, time, type, std::move(passed_reasons), page_id);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(ui_service()->decision_reasons(), ::testing::ElementsAre(reason));
  EXPECT_THAT(ui_service()->decision_urls(), ::testing::ElementsAre(url));
  EXPECT_THAT(ui_service()->decision_types(), ::testing::ElementsAre(type));
  EXPECT_THAT(ui_service()->decision_times(), ::testing::ElementsAre(time));
  EXPECT_THAT(ui_service()->decision_ids(), ::testing::ElementsAre(page_id));

  auto actual_passed_reasons = ui_service()->decision_passed_reasons();
  EXPECT_EQ(1UL, actual_passed_reasons.size());
  EXPECT_EQ(expected_passed_reasons.size(), actual_passed_reasons[0].size());
  for (size_t i = 0; i < actual_passed_reasons[0].size(); i++) {
    EXPECT_EQ(expected_passed_reasons[i], actual_passed_reasons[0][i]);
  }
}  // namespace

TEST_F(PreviewsDeciderImplTest, LogDecisionMadeBlacklistNotAvailable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kClientLoFi}, {});

  InitializeUIService();
  auto expected_reason = PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE;
  auto expected_type = PreviewsType::LOFI;

  previews_decider_impl()->InjectTestBlacklist(nullptr /* blacklist */);
  PreviewsUserData user_data(kDefaultPageId);
  previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("http://www.google.com"), false, expected_type,
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN, {}, false);
  base::RunLoop().RunUntilIdle();
  // Testing correct log method is called.
  EXPECT_THAT(ui_service()->decision_reasons(),
              ::testing::Contains(expected_reason));
  EXPECT_THAT(ui_service()->decision_types(),
              ::testing::Contains(expected_type));
}

TEST_F(PreviewsDeciderImplTest, LogDecisionMadeBlacklistStatusesDefault) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kClientLoFi}, {});
  InitializeUIService();

  PreviewsEligibilityReason expected_reasons[] = {
      PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
      PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
      PreviewsEligibilityReason::USER_BLACKLISTED,
      PreviewsEligibilityReason::HOST_BLACKLISTED,
  };

  auto expected_type = PreviewsType::LOFI;
  const size_t reasons_size = 4;

  for (size_t i = 0; i < reasons_size; i++) {
    auto expected_reason = expected_reasons[i];

    std::unique_ptr<TestPreviewsBlackList> blacklist =
        std::make_unique<TestPreviewsBlackList>(expected_reason,
                                                previews_decider_impl());
    previews_decider_impl()->InjectTestBlacklist(std::move(blacklist));

    PreviewsUserData user_data(kDefaultPageId);
    previews_decider_impl()->ShouldAllowPreviewAtECT(
        &user_data, GURL("http://www.google.com"), false, expected_type,
        net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN, {}, false);
    base::RunLoop().RunUntilIdle();
    // Testing correct log method is called.
    // Check for all decision upto current decision is logged.
    for (size_t j = 0; j <= i; j++) {
      EXPECT_THAT(ui_service()->decision_reasons(),
                  ::testing::Contains(expected_reasons[j]));
    }
    EXPECT_THAT(ui_service()->decision_types(),
                ::testing::Contains(expected_type));
  }
}

TEST_F(PreviewsDeciderImplTest, IsURLAllowedForPreviewBlacklistStatuses) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kNoScriptPreviews},
      {features::kOptimizationHints});
  InitializeUIService();
  auto expected_type = PreviewsType::NOSCRIPT;
  PreviewsUserData user_data(kDefaultPageId);
  // First verify URL is allowed for no blacklist status.
  EXPECT_TRUE(previews_decider_impl()->IsURLAllowedForPreview(
      &user_data, GURL("http://www.google.com"), expected_type));

  PreviewsEligibilityReason expected_reasons[] = {
      PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
      PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
      PreviewsEligibilityReason::USER_BLACKLISTED,
      PreviewsEligibilityReason::HOST_BLACKLISTED,
  };

  const size_t reasons_size = 4;

  for (size_t i = 0; i < reasons_size; i++) {
    auto expected_reason = expected_reasons[i];

    std::unique_ptr<TestPreviewsBlackList> blacklist =
        std::make_unique<TestPreviewsBlackList>(expected_reason,
                                                previews_decider_impl());
    previews_decider_impl()->InjectTestBlacklist(std::move(blacklist));
    PreviewsUserData user_data(kDefaultPageId);
    EXPECT_FALSE(previews_decider_impl()->IsURLAllowedForPreview(
        &user_data, GURL("http://www.google.com"), expected_type));
    base::RunLoop().RunUntilIdle();
    // Testing correct log method is called.
    // Check for all decision upto current decision is logged.
    for (size_t j = 0; j <= i; j++) {
      EXPECT_THAT(ui_service()->decision_reasons(),
                  ::testing::Contains(expected_reasons[j]));
    }
    EXPECT_THAT(ui_service()->decision_types(),
                ::testing::Contains(expected_type));
  }
}

TEST_F(PreviewsDeciderImplTest, LogDecisionMadeBlacklistStatusesIgnore) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kClientLoFi}, {});
  InitializeUIService();
  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);
  auto expected_reason = PreviewsEligibilityReason::ALLOWED;
  auto expected_type = PreviewsType::LOFI;

  PreviewsEligibilityReason blacklist_decisions[] = {
      PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
      PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
      PreviewsEligibilityReason::USER_BLACKLISTED,
      PreviewsEligibilityReason::HOST_BLACKLISTED,
  };

  previews_decider_impl()->SetIgnorePreviewsBlacklistDecision(
      true /* ignored */);

  for (auto blacklist_decision : blacklist_decisions) {
    std::unique_ptr<TestPreviewsBlackList> blacklist =
        std::make_unique<TestPreviewsBlackList>(blacklist_decision,
                                                previews_decider_impl());
    previews_decider_impl()->InjectTestBlacklist(std::move(blacklist));
    PreviewsUserData user_data(kDefaultPageId);
    previews_decider_impl()->ShouldAllowPreviewAtECT(
        &user_data, GURL("http://www.google.com"), false, expected_type,
        params::EffectiveConnectionTypeThresholdForClientLoFi(),
        params::GetBlackListedHostsForClientLoFiFieldTrial(), false);

    base::RunLoop().RunUntilIdle();
    // Testing correct log method is called.
    EXPECT_THAT(ui_service()->decision_reasons(),
                ::testing::Contains(expected_reason));
    EXPECT_THAT(ui_service()->decision_types(),
                ::testing::Contains(expected_type));
  }
}

TEST_F(PreviewsDeciderImplTest, IgnoreFlagStillHasFiveSecondRule) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kClientLoFi}, {});
  InitializeUIService();
  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  previews_decider_impl()->SetIgnorePreviewsBlacklistDecision(
      true /* ignored */);
  PreviewsUserData user_data(kDefaultPageId);
  EXPECT_TRUE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false, PreviewsType::LOFI,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial(), false));

  previews_decider_impl()->AddPreviewNavigation(
      GURL("http://wwww.somedomain.com"), true, PreviewsType::LOFI, 1);

  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false, PreviewsType::LOFI,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial(), false));

  clock_.Advance(base::TimeDelta::FromSeconds(6));

  EXPECT_TRUE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false, PreviewsType::LOFI,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial(), false));
}

TEST_F(PreviewsDeciderImplTest, LogDecisionMadeNetworkQualityNotAvailable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kClientLoFi}, {});
  InitializeUIService();
  std::unique_ptr<TestPreviewsBlackList> blacklist =
      std::make_unique<TestPreviewsBlackList>(
          PreviewsEligibilityReason::ALLOWED, previews_decider_impl());
  previews_decider_impl()->InjectTestBlacklist(std::move(blacklist));

  auto expected_reason = PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE;
  auto expected_type = PreviewsType::LOFI;

  std::vector<PreviewsEligibilityReason> checked_decisions = {
      PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE,
      PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
      PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
      PreviewsEligibilityReason::USER_BLACKLISTED,
      PreviewsEligibilityReason::HOST_BLACKLISTED,
  };

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  PreviewsUserData user_data(kDefaultPageId);
  previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("http://www.google.com"), false, expected_type,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial(), false);

  base::RunLoop().RunUntilIdle();
  // Testing correct log method is called.
  EXPECT_THAT(ui_service()->decision_reasons(),
              ::testing::Contains(expected_reason));
  EXPECT_THAT(ui_service()->decision_types(),
              ::testing::Contains(expected_type));

  EXPECT_EQ(1UL, ui_service()->decision_passed_reasons().size());
  auto actual_passed_reasons = ui_service()->decision_passed_reasons()[0];
  EXPECT_EQ(checked_decisions.size(), actual_passed_reasons.size());
  EXPECT_EQ(checked_decisions.size(), actual_passed_reasons.size());
  for (size_t i = 0; i < actual_passed_reasons.size(); i++) {
    EXPECT_EQ(checked_decisions[i], actual_passed_reasons[i]);
  }
}

TEST_F(PreviewsDeciderImplTest, LogDecisionMadeNetworkNotSlow) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kClientLoFi}, {});
  InitializeUIService();
  std::unique_ptr<TestPreviewsBlackList> blacklist =
      std::make_unique<TestPreviewsBlackList>(
          PreviewsEligibilityReason::ALLOWED, previews_decider_impl());
  previews_decider_impl()->InjectTestBlacklist(std::move(blacklist));

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_4G);

  auto expected_reason = PreviewsEligibilityReason::NETWORK_NOT_SLOW;
  auto expected_type = PreviewsType::LOFI;

  std::vector<PreviewsEligibilityReason> checked_decisions = {
      PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE,
      PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
      PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
      PreviewsEligibilityReason::USER_BLACKLISTED,
      PreviewsEligibilityReason::HOST_BLACKLISTED,
      PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE,
  };
  PreviewsUserData user_data(kDefaultPageId);
  previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("http://www.google.com"), false, expected_type,
      net::EFFECTIVE_CONNECTION_TYPE_2G /* threshold */, {}, false);
  base::RunLoop().RunUntilIdle();
  // Testing correct log method is called.
  EXPECT_THAT(ui_service()->decision_reasons(),
              ::testing::Contains(expected_reason));
  EXPECT_THAT(ui_service()->decision_types(),
              ::testing::Contains(expected_type));

  EXPECT_EQ(1UL, ui_service()->decision_passed_reasons().size());
  auto actual_passed_reasons = ui_service()->decision_passed_reasons()[0];
  EXPECT_EQ(checked_decisions.size(), actual_passed_reasons.size());
  EXPECT_EQ(checked_decisions.size(), actual_passed_reasons.size());
  for (size_t i = 0; i < actual_passed_reasons.size(); i++) {
    EXPECT_EQ(checked_decisions[i], actual_passed_reasons[i]);
  }
}

TEST_F(PreviewsDeciderImplTest, LogDecisionMadeHostBlacklisted) {
  base::test::ScopedFeatureList scoped_previews_feature_list;
  scoped_previews_feature_list.InitAndEnableFeature(features::kPreviews);

  // Use a nested ScopedFeatureList in order to set parameters.
  base::test::ScopedFeatureList scoped_lofi_feature_list;
  scoped_lofi_feature_list.InitAndEnableFeatureWithParameters(
      features::kClientLoFi, {{"short_host_blacklist", "example.com"}});

  InitializeUIService();
  std::unique_ptr<TestPreviewsBlackList> blacklist =
      std::make_unique<TestPreviewsBlackList>(
          PreviewsEligibilityReason::ALLOWED, previews_decider_impl());
  previews_decider_impl()->InjectTestBlacklist(std::move(blacklist));

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  auto expected_reason = PreviewsEligibilityReason::HOST_BLACKLISTED_BY_SERVER;
  auto expected_type = PreviewsType::LOFI;

  std::vector<PreviewsEligibilityReason> checked_decisions = {
      PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE,
      PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
      PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
      PreviewsEligibilityReason::USER_BLACKLISTED,
      PreviewsEligibilityReason::HOST_BLACKLISTED,
      PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE,
      PreviewsEligibilityReason::NETWORK_NOT_SLOW,
      PreviewsEligibilityReason::RELOAD_DISALLOWED,
  };
  PreviewsUserData user_data(kDefaultPageId);
  previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("http://example.com"), false, expected_type,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial(), false);
  base::RunLoop().RunUntilIdle();

  // Testing correct log method is called.
  EXPECT_THAT(ui_service()->decision_reasons(),
              ::testing::Contains(expected_reason));
  EXPECT_THAT(ui_service()->decision_types(),
              ::testing::Contains(expected_type));

  EXPECT_EQ(1UL, ui_service()->decision_passed_reasons().size());
  auto actual_passed_reasons = ui_service()->decision_passed_reasons()[0];
  EXPECT_EQ(checked_decisions.size(), actual_passed_reasons.size());
  EXPECT_EQ(checked_decisions.size(), actual_passed_reasons.size());
  for (size_t i = 0; i < actual_passed_reasons.size(); i++) {
    EXPECT_EQ(checked_decisions[i], actual_passed_reasons[i]);
  }
}

TEST_F(PreviewsDeciderImplTest, LogDecisionMadeReloadDisallowed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPreviews);
  InitializeUIService();
  std::unique_ptr<TestPreviewsBlackList> blacklist =
      std::make_unique<TestPreviewsBlackList>(
          PreviewsEligibilityReason::ALLOWED, previews_decider_impl());
  previews_decider_impl()->InjectTestBlacklist(std::move(blacklist));

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);
  PreviewsUserData user_data(kDefaultPageId);

  auto expected_reason = PreviewsEligibilityReason::RELOAD_DISALLOWED;
  auto expected_type = PreviewsType::OFFLINE;

  std::vector<PreviewsEligibilityReason> checked_decisions = {
      PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE,
      PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
      PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
      PreviewsEligibilityReason::USER_BLACKLISTED,
      PreviewsEligibilityReason::HOST_BLACKLISTED,
      PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE,
      PreviewsEligibilityReason::NETWORK_NOT_SLOW,
  };

  previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("http://www.google.com"), true, expected_type,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial(), false);
  base::RunLoop().RunUntilIdle();

  // Testing correct log method is called.
  EXPECT_THAT(ui_service()->decision_reasons(),
              ::testing::Contains(expected_reason));
  EXPECT_THAT(ui_service()->decision_types(),
              ::testing::Contains(expected_type));

  EXPECT_EQ(1UL, ui_service()->decision_passed_reasons().size());
  auto actual_passed_reasons = ui_service()->decision_passed_reasons()[0];
  EXPECT_EQ(checked_decisions.size(), actual_passed_reasons.size());
  EXPECT_EQ(checked_decisions.size(), actual_passed_reasons.size());
  for (size_t i = 0; i < actual_passed_reasons.size(); i++) {
    EXPECT_EQ(checked_decisions[i], actual_passed_reasons[i]);
  }
}

TEST_F(PreviewsDeciderImplTest, IgnoreBlacklistEnabledViaFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kClientLoFi}, {});
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitch(switches::kIgnorePreviewsBlacklist);
  ASSERT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kIgnorePreviewsBlacklist));

  InitializeUIService();

  std::unique_ptr<TestPreviewsBlackList> blacklist =
      std::make_unique<TestPreviewsBlackList>(
          PreviewsEligibilityReason::HOST_BLACKLISTED, previews_decider_impl());
  previews_decider_impl()->InjectTestBlacklist(std::move(blacklist));
  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);
  PreviewsUserData user_data(kDefaultPageId);
  auto expected_reason = PreviewsEligibilityReason::ALLOWED;
  EXPECT_TRUE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false, PreviewsType::LOFI,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial(), false));

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(ui_service()->decision_reasons(),
              ::testing::Contains(expected_reason));
}

TEST_F(PreviewsDeciderImplTest, LogDecisionMadeAllowPreviewsOnECT) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kClientLoFi}, {});
  InitializeUIService();

  std::unique_ptr<TestPreviewsBlackList> blacklist =
      std::make_unique<TestPreviewsBlackList>(
          PreviewsEligibilityReason::ALLOWED, previews_decider_impl());

  previews_decider_impl()->InjectTestBlacklist(std::move(blacklist));

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  auto expected_reason = PreviewsEligibilityReason::ALLOWED;
  auto expected_type = PreviewsType::LOFI;

  std::vector<PreviewsEligibilityReason> checked_decisions = {
      PreviewsEligibilityReason::BLACKLIST_UNAVAILABLE,
      PreviewsEligibilityReason::BLACKLIST_DATA_NOT_LOADED,
      PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
      PreviewsEligibilityReason::USER_BLACKLISTED,
      PreviewsEligibilityReason::HOST_BLACKLISTED,
      PreviewsEligibilityReason::NETWORK_QUALITY_UNAVAILABLE,
      PreviewsEligibilityReason::NETWORK_NOT_SLOW,
      PreviewsEligibilityReason::RELOAD_DISALLOWED,
      PreviewsEligibilityReason::HOST_BLACKLISTED_BY_SERVER,
  };
  PreviewsUserData user_data(kDefaultPageId);
  previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("http://www.google.com"), false, expected_type,
      params::EffectiveConnectionTypeThresholdForClientLoFi(),
      params::GetBlackListedHostsForClientLoFiFieldTrial(), false);
  base::RunLoop().RunUntilIdle();

  // Testing correct log method is called.
  EXPECT_THAT(ui_service()->decision_reasons(),
              ::testing::Contains(expected_reason));
  EXPECT_THAT(ui_service()->decision_types(),
              ::testing::Contains(expected_type));

  EXPECT_EQ(1UL, ui_service()->decision_passed_reasons().size());
  auto actual_passed_reasons = ui_service()->decision_passed_reasons()[0];
  EXPECT_EQ(checked_decisions.size(), actual_passed_reasons.size());
  EXPECT_EQ(checked_decisions.size(), actual_passed_reasons.size());
  for (size_t i = 0; i < actual_passed_reasons.size(); i++) {
    EXPECT_EQ(checked_decisions[i], actual_passed_reasons[i]);
  }
}

TEST_F(PreviewsDeciderImplTest, OnNewBlacklistedHostCallsUIMethodCorrectly) {
  InitializeUIService();
  std::string expected_host = "example.com";
  base::Time expected_time = base::Time::Now();
  previews_decider_impl()->OnNewBlacklistedHost(expected_host, expected_time);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(expected_host, ui_service()->host_blacklisted());
  EXPECT_EQ(expected_time, ui_service()->host_blacklisted_time());
}

TEST_F(PreviewsDeciderImplTest, OnUserBlacklistedCallsUIMethodCorrectly) {
  InitializeUIService();
  previews_decider_impl()->OnUserBlacklistedStatusChange(
      true /* blacklisted */);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ui_service()->user_blacklisted());

  previews_decider_impl()->OnUserBlacklistedStatusChange(
      false /* blacklisted */);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(ui_service()->user_blacklisted());
}

TEST_F(PreviewsDeciderImplTest, OnBlacklistClearedCallsUIMethodCorrectly) {
  InitializeUIService();
  base::Time expected_time = base::Time::Now();
  previews_decider_impl()->OnBlacklistCleared(expected_time);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(expected_time, ui_service()->blacklist_cleared_time());
}

TEST_F(PreviewsDeciderImplTest,
       OnIgnoreBlacklistDecisionStatusChangedCalledCorrect) {
  InitializeUIService();
  previews_decider_impl()->SetIgnorePreviewsBlacklistDecision(
      true /* ignored */);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ui_service()->blacklist_ignored());

  previews_decider_impl()->SetIgnorePreviewsBlacklistDecision(
      false /* ignored */);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ui_service()->blacklist_ignored());
}

TEST_F(PreviewsDeciderImplTest, GeneratePageIdMakesUniqueNonZero) {
  InitializeUIService();
  std::unordered_set<uint64_t> page_id_set;
  size_t number_of_generated_ids = 10;
  for (size_t i = 0; i < number_of_generated_ids; i++) {
    page_id_set.insert(previews_decider_impl()->GeneratePageId());
  }
  EXPECT_EQ(number_of_generated_ids, page_id_set.size());
  EXPECT_EQ(page_id_set.end(), page_id_set.find(0u));
}

TEST_F(PreviewsDeciderImplTest, TestIgnoreLongTermRule) {
  // Verify that when long term rules can be ignored, and the caller is fine
  // with ignoring long term rules, they are not checked.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPreviews);
  InitializeUIService();

  previews_decider_impl()->SetIgnoreLongTermBlackListForServerPreviews(true);

  std::unique_ptr<TestPreviewsBlackList> blacklist =
      std::make_unique<TestPreviewsBlackList>(
          PreviewsEligibilityReason::HOST_BLACKLISTED, previews_decider_impl());
  previews_decider_impl()->InjectTestBlacklist(std::move(blacklist));

  // LoFi and LitePage check NQE on their own.
  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_3G);
  PreviewsUserData user_data(kDefaultPageId);
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false,
      PreviewsType::LITE_PAGE, net::EFFECTIVE_CONNECTION_TYPE_4G,
      std::vector<std::string>(), false));
  EXPECT_TRUE(previews_decider_impl()->ShouldAllowPreviewAtECT(
      &user_data, GURL("https://www.google.com"), false,
      PreviewsType::LITE_PAGE, net::EFFECTIVE_CONNECTION_TYPE_4G,
      std::vector<std::string>(), true));
}

}  // namespace

}  // namespace previews
