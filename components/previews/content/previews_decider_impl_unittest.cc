// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_decider_impl.h"

#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_data.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_delegate.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_item.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/optimization_guide/content/browser/test_optimization_guide_decider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_block_list.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace previews {

namespace {

// A fake default page_id for testing.
const uint64_t kDefaultPageId = 123456;

// This method simulates the actual behavior of the passed in callback, which is
// validated in other tests. For simplicity, offline, and lite page use the
// offline previews check.
bool IsPreviewFieldTrialEnabled(PreviewsType type) {
  switch (type) {
    case PreviewsType::DEFER_ALL_SCRIPT:
      return params::IsDeferAllScriptPreviewsEnabled();
    case PreviewsType::NONE:
    case PreviewsType::UNSPECIFIED:
    case PreviewsType::LAST:
      break;
  }
  NOTREACHED();
  return false;
}

// Stub class of PreviewsBlockList to control IsLoadedAndAllowed outcome when
// testing PreviewsDeciderImpl.
class TestPreviewsBlockList : public PreviewsBlockList {
 public:
  TestPreviewsBlockList(PreviewsEligibilityReason status,
                        blocklist::OptOutBlocklistDelegate* blocklist_delegate)
      : PreviewsBlockList(nullptr,
                          base::DefaultClock::GetInstance(),
                          blocklist_delegate,
                          {}),
        status_(status) {}
  ~TestPreviewsBlockList() override = default;

  // PreviewsBlockList:
  PreviewsEligibilityReason IsLoadedAndAllowed(
      const GURL& url,
      PreviewsType type,
      std::vector<PreviewsEligibilityReason>* passed_reasons) const override {
    std::vector<PreviewsEligibilityReason> ordered_reasons = {
        PreviewsEligibilityReason::BLOCKLIST_DATA_NOT_LOADED,
        PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
        PreviewsEligibilityReason::USER_BLOCKLISTED,
        PreviewsEligibilityReason::HOST_BLOCKLISTED};

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

// Stub class of PreviewsOptimizationGuide to control what is allowed when
// testing PreviewsDecider.
class TestPreviewsOptimizationGuide
    : public PreviewsOptimizationGuide,
      public network::NetworkQualityTracker::EffectiveConnectionTypeObserver {
 public:
  TestPreviewsOptimizationGuide(
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      network::NetworkQualityTracker* network_quality_tracker)
      : PreviewsOptimizationGuide(optimization_guide_decider),
        network_quality_tracker_(network_quality_tracker) {
    network_quality_tracker_->AddEffectiveConnectionTypeObserver(this);
  }

  ~TestPreviewsOptimizationGuide() override {
    network_quality_tracker_->RemoveEffectiveConnectionTypeObserver(this);
  }

  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType ect) override {
    current_ect_ = ect;
  }

  bool ShouldShowPreview(
      content::NavigationHandle* navigation_handle) override {
    return current_ect_ <= net::EFFECTIVE_CONNECTION_TYPE_2G;
  }

  // PreviewsOptimizationGuide:
  bool CanApplyPreview(PreviewsUserData* previews_user_data,
                       content::NavigationHandle* navigation_handle,
                       PreviewsType type) override {
    EXPECT_TRUE(type == PreviewsType::DEFER_ALL_SCRIPT);

    const GURL url = navigation_handle->GetURL();
    if (type == PreviewsType::DEFER_ALL_SCRIPT) {
      return url.host().compare("allowlisted.example.com") == 0;
    }
    return false;
  }

  // Returns whether the URL associated with |navigation_handle| should be
  // blocklisted from |type|.
  bool IsBlocklisted(content::NavigationHandle* navigation_handle,
                     PreviewsType type) const {
    return false;
  }

 private:
  network::NetworkQualityTracker* network_quality_tracker_;
  net::EffectiveConnectionType current_ect_ =
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
};

// Stub class of PreviewsUIService to test logging functionalities in
// PreviewsDeciderImpl.
class TestPreviewsUIService : public PreviewsUIService {
 public:
  TestPreviewsUIService(
      std::unique_ptr<PreviewsDeciderImpl> previews_decider_impl,
      std::unique_ptr<blocklist::OptOutStore> previews_opt_out_store,
      std::unique_ptr<PreviewsOptimizationGuide> previews_opt_guide,
      const PreviewsIsEnabledCallback& is_enabled_callback,
      blocklist::BlocklistData::AllowedTypesAndVersions allowed_types,
      network::NetworkQualityTracker* network_quality_tracker)
      : PreviewsUIService(std::move(previews_decider_impl),
                          std::move(previews_opt_out_store),
                          std::move(previews_opt_guide),
                          is_enabled_callback,
                          std::move(allowed_types),
                          network_quality_tracker),
        user_blocklisted_(false),
        blocklist_ignored_(false) {}

  // PreviewsUIService:
  void OnNewBlocklistedHost(const std::string& host, base::Time time) override {
    host_blocklisted_ = host;
    host_blocklisted_time_ = time;
  }
  void OnUserBlocklistedStatusChange(bool blocklisted) override {
    user_blocklisted_ = blocklisted;
  }
  void OnBlocklistCleared(base::Time time) override {
    blocklist_cleared_time_ = time;
  }
  void OnIgnoreBlocklistDecisionStatusChanged(bool ignored) override {
    blocklist_ignored_ = ignored;
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

  // Expose passed in params for hosts and user blocklist event.
  std::string host_blocklisted() const { return host_blocklisted_; }
  base::Time host_blocklisted_time() const { return host_blocklisted_time_; }
  bool user_blocklisted() const { return user_blocklisted_; }
  base::Time blocklist_cleared_time() const { return blocklist_cleared_time_; }

  // Expose the status of blocklist decisions ignored.
  bool blocklist_ignored() const { return blocklist_ignored_; }

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
    LOG(INFO) << "Decision is logged";
    decision_reasons_.push_back(reason);
    decision_urls_.push_back(GURL(url));
    decision_times_.push_back(time);
    decision_types_.push_back(type);
    decision_passed_reasons_.push_back(std::move(passed_reasons));
    decision_ids_.push_back(page_id);
  }

  // Passed in params for blocklist status events.
  std::string host_blocklisted_;
  base::Time host_blocklisted_time_;
  bool user_blocklisted_;
  base::Time blocklist_cleared_time_;

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

  // Whether the blocklist decisions are ignored or not.
  bool blocklist_ignored_;
};

class TestPreviewsDeciderImpl : public PreviewsDeciderImpl {
 public:
  explicit TestPreviewsDeciderImpl(base::Clock* clock)
      : PreviewsDeciderImpl(clock) {}
  ~TestPreviewsDeciderImpl() override {}

  // Expose the injecting blocklist method from PreviewsDeciderImpl, and inject
  // |blocklist| into |this|.
  void InjectTestBlocklist(std::unique_ptr<PreviewsBlockList> blocklist) {
    SetPreviewsBlocklistForTesting(std::move(blocklist));
  }
};

void RunLoadCallback(blocklist::LoadBlockListCallback callback,
                     std::unique_ptr<blocklist::BlocklistData> data) {
  std::move(callback).Run(std::move(data));
}

class TestOptOutStore : public blocklist::OptOutStore {
 public:
  TestOptOutStore() {}
  ~TestOptOutStore() override {}

 private:
  // blocklist::OptOutStore implementation:
  void AddEntry(bool opt_out,
                const std::string& host_name,
                int type,
                base::Time now) override {}

  void LoadBlockList(std::unique_ptr<blocklist::BlocklistData> data,
                     blocklist::LoadBlockListCallback callback) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&RunLoadCallback, std::move(callback), std::move(data)));
  }

  void ClearBlockList(base::Time begin_time, base::Time end_time) override {}
};

class PreviewsDeciderImplTest : public testing::Test {
 public:
  PreviewsDeciderImplTest() : previews_decider_impl_(nullptr) {
    clock_.SetNow(base::Time::Now());

    network_quality_tracker_.ReportEffectiveConnectionTypeForTesting(
        net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  }

  ~PreviewsDeciderImplTest() override {
    // TODO(dougarnett) bug 781975: Consider switching to Feature API and
    // ScopedFeatureList (and dropping components/variations dep).
    variations::testing::ClearAllVariationParams();
  }

  void SetUp() override {
    // Enable DataSaver for checks with PreviewsOptimizationGuide.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        data_reduction_proxy::switches::kEnableDataReductionProxy);
  }

  void TearDown() override {
    ui_service_.reset();
  }

  void InitializeUIServiceWithoutWaitingForBlockList(
      bool include_previews_opt_guide) {
    blocklist::BlocklistData::AllowedTypesAndVersions allowed_types;
    allowed_types[static_cast<int>(PreviewsType::DEFER_ALL_SCRIPT)] = 0;

    std::unique_ptr<TestPreviewsDeciderImpl> previews_decider_impl =
        std::make_unique<TestPreviewsDeciderImpl>(&clock_);
    previews_decider_impl_ = previews_decider_impl.get();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    std::unique_ptr<TestPreviewsOptimizationGuide> previews_opt_guide =
        std::make_unique<TestPreviewsOptimizationGuide>(
            &optimization_guide_decider_, &network_quality_tracker_);
    previews_opt_guide_ = previews_opt_guide.get();
    ui_service_.reset(new TestPreviewsUIService(
        std::move(previews_decider_impl), std::make_unique<TestOptOutStore>(),
        include_previews_opt_guide ? std::move(previews_opt_guide) : nullptr,
        base::BindRepeating(&IsPreviewFieldTrialEnabled),
        std::move(allowed_types), &network_quality_tracker_));
  }

  void InitializeUIService(bool include_previews_opt_guide = true) {
    InitializeUIServiceWithoutWaitingForBlockList(include_previews_opt_guide);
    task_environment_.RunUntilIdle();
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
  base::test::TaskEnvironment task_environment_;
  TestPreviewsDeciderImpl* previews_decider_impl_;
  optimization_guide::TestOptimizationGuideDecider optimization_guide_decider_;
  TestPreviewsOptimizationGuide* previews_opt_guide_;
  std::unique_ptr<TestPreviewsUIService> ui_service_;
  network::TestNetworkQualityTracker network_quality_tracker_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  scoped_refptr<network::TestSharedURLLoaderFactory> url_loader_factory_;
};

TEST_F(PreviewsDeciderImplTest, AllPreviewsDisabledByFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kDeferAllScriptPreviews},
      {features::kPreviews} /* disable_features */);
  InitializeUIService();

  PreviewsUserData user_data(kDefaultPageId);

  PreviewsUserData user_data2(kDefaultPageId);
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://www.google.com"));
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtNavigationStart(
      &user_data2, &navigation_handle, false, PreviewsType::DEFER_ALL_SCRIPT));
}

TEST_F(PreviewsDeciderImplTest, TestDisallowBasicAuthentication) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kDeferAllScriptPreviews}, {});
  InitializeUIService();
  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  base::HistogramTester histogram_tester;
  PreviewsUserData user_data(kDefaultPageId);
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://user:pass@www.google.com"));
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtNavigationStart(
      &user_data, &navigation_handle, false, PreviewsType::DEFER_ALL_SCRIPT));
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason",
      static_cast<int>(PreviewsEligibilityReason::URL_HAS_BASIC_AUTH), 1);
  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(PreviewsEligibilityReason::URL_HAS_BASIC_AUTH), 1);
}

TEST_F(PreviewsDeciderImplTest, TestDisallowOnReload) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kDeferAllScriptPreviews}, {});
  InitializeUIService();

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  PreviewsUserData user_data(kDefaultPageId);
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://www.google.com"));

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtNavigationStart(
      &user_data, &navigation_handle, true, PreviewsType::DEFER_ALL_SCRIPT));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason",
      static_cast<int>(PreviewsEligibilityReason::RELOAD_DISALLOWED), 1);
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(PreviewsEligibilityReason::RELOAD_DISALLOWED), 1);
}

TEST_F(PreviewsDeciderImplTest, MissingHostDisallowed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kDeferAllScriptPreviews}, {});
  InitializeUIService();

  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            params::GetECTThresholdForPreview(PreviewsType::DEFER_ALL_SCRIPT));
  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  PreviewsUserData user_data(kDefaultPageId);
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("file:///sdcard"));
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtNavigationStart(
      &user_data, &navigation_handle, false, PreviewsType::DEFER_ALL_SCRIPT));
}

TEST_F(PreviewsDeciderImplTest, DeferAllScriptDefaultBehavior) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPreviews);
  InitializeUIService();

  base::HistogramTester histogram_tester;
  PreviewsUserData user_data(kDefaultPageId);
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://www.google.com"));

  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtNavigationStart(
      &user_data, &navigation_handle, false, PreviewsType::DEFER_ALL_SCRIPT));
}

TEST_F(PreviewsDeciderImplTest,
       DeferAllScriptDisallowedWithoutOptimizationHints) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kDeferAllScriptPreviews}, {});
  InitializeUIService(/*include_previews_opt_guide=*/false);

  base::HistogramTester histogram_tester;
  PreviewsUserData user_data(kDefaultPageId);
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://allowlisted.example.com"));
  EXPECT_FALSE(previews_decider_impl()->ShouldAllowPreviewAtNavigationStart(
      &user_data, &navigation_handle, false, PreviewsType::DEFER_ALL_SCRIPT));
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason",
      static_cast<int>(
          PreviewsEligibilityReason::OPTIMIZATION_HINTS_NOT_AVAILABLE),
      1);
  histogram_tester.ExpectUniqueSample(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(
          PreviewsEligibilityReason::OPTIMIZATION_HINTS_NOT_AVAILABLE),
      1);
}

TEST_F(PreviewsDeciderImplTest, DeferAllScriptAllowedByFeatureAndAllowlist) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kDeferAllScriptPreviews}, {});
  InitializeUIService();

  for (const auto& test_ect : {net::EFFECTIVE_CONNECTION_TYPE_OFFLINE,
                               net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
                               net::EFFECTIVE_CONNECTION_TYPE_3G}) {
    ReportEffectiveConnectionType(test_ect);

    base::HistogramTester histogram_tester;
    PreviewsUserData user_data(kDefaultPageId);
    content::MockNavigationHandle navigation_handle;
    navigation_handle.set_url(GURL("https://allowlisted.example.com"));

    // Check allowlisted URL.
    EXPECT_TRUE(previews_decider_impl()->ShouldAllowPreviewAtNavigationStart(
        &user_data, &navigation_handle, false, PreviewsType::DEFER_ALL_SCRIPT));
    EXPECT_EQ(test_ect, user_data.navigation_ect());
    histogram_tester.ExpectUniqueSample(
        "Previews.EligibilityReason.DeferAllScript",
        static_cast<int>(PreviewsEligibilityReason::ALLOWED), 1);
  }
}

TEST_F(PreviewsDeciderImplTest,
       DeferAllScriptAllowedByFeatureWithoutKnownHints) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kDeferAllScriptPreviews}, {});
  InitializeUIService();

  base::HistogramTester histogram_tester;
  PreviewsUserData user_data(kDefaultPageId);
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://www.google.com"));

  // Verify preview allowed initially for url without known hints.
  EXPECT_TRUE(previews_decider_impl()->ShouldAllowPreviewAtNavigationStart(
      &user_data, &navigation_handle, false, PreviewsType::DEFER_ALL_SCRIPT));

  histogram_tester.ExpectBucketCount(
      "Previews.EligibilityReason.DeferAllScript",
      static_cast<int>(PreviewsEligibilityReason::ALLOWED), 1);
}

TEST_F(PreviewsDeciderImplTest, DeferAllScriptCommitTimeAllowlistCheck) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kDeferAllScriptPreviews}, {});
  InitializeUIService();

  // First verify not allowed for non-allowlisted url.
  {
    ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);
    base::HistogramTester histogram_tester;
    PreviewsUserData user_data(kDefaultPageId);
    content::MockNavigationHandle navigation_handle;
    navigation_handle.set_url(GURL("https://www.google.com"));
    EXPECT_FALSE(previews_decider_impl()->ShouldCommitPreview(
        &user_data, &navigation_handle, PreviewsType::DEFER_ALL_SCRIPT));

    histogram_tester.ExpectUniqueSample(
        "Previews.EligibilityReason.DeferAllScript",
        static_cast<int>(
            PreviewsEligibilityReason::NOT_ALLOWED_BY_OPTIMIZATION_GUIDE),
        1);
  }

  // Now verify preview for allowlisted url.
  {
    ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);
    base::HistogramTester histogram_tester;
    PreviewsUserData user_data(kDefaultPageId);
    content::MockNavigationHandle navigation_handle;
    navigation_handle.set_url(GURL("https://allowlisted.example.com"));
    EXPECT_TRUE(previews_decider_impl()->ShouldCommitPreview(
        &user_data, &navigation_handle, PreviewsType::DEFER_ALL_SCRIPT));

    // Expect no eligibility logging.
    histogram_tester.ExpectTotalCount(
        "Previews.EligibilityReason.DeferAllScript", 0);
  }

  // Verify preview not allowed for allowlisted url when network is not slow.
  {
    ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_4G);
    base::HistogramTester histogram_tester;
    PreviewsUserData user_data(kDefaultPageId);
    content::MockNavigationHandle navigation_handle;
    navigation_handle.set_url(GURL("https://allowlisted.example.com"));
    EXPECT_FALSE(previews_decider_impl()->ShouldCommitPreview(
        &user_data, &navigation_handle, PreviewsType::DEFER_ALL_SCRIPT));

    histogram_tester.ExpectUniqueSample(
        "Previews.EligibilityReason.DeferAllScript",
        static_cast<int>(
            PreviewsEligibilityReason::PAGE_LOAD_PREDICTION_NOT_PAINFUL),
        1);
  }

  // Verify preview not allowed for allowlisted url for offline network quality.
  {
    ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
    base::HistogramTester histogram_tester;
    PreviewsUserData user_data(kDefaultPageId);
    content::MockNavigationHandle navigation_handle;
    navigation_handle.set_url(GURL("https://allowlisted.example.com"));
    EXPECT_FALSE(previews_decider_impl()->ShouldCommitPreview(
        &user_data, &navigation_handle, PreviewsType::DEFER_ALL_SCRIPT));

    histogram_tester.ExpectUniqueSample(
        "Previews.EligibilityReason.DeferAllScript",
        static_cast<int>(PreviewsEligibilityReason::DEVICE_OFFLINE), 1);
  }
}

TEST_F(PreviewsDeciderImplTest, LogPreviewNavigationPassInCorrectParams) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPreviews);
  InitializeUIService();
  const GURL url("http://www.url_a.com/url_a");
  const bool opt_out = true;
  const PreviewsType type = PreviewsType::DEFER_ALL_SCRIPT;
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
      PreviewsEligibilityReason::BLOCKLIST_UNAVAILABLE);
  const GURL url("http://www.url_a.com/url_a");
  const base::Time time = base::Time::Now();
  const PreviewsType type = PreviewsType::DEFER_ALL_SCRIPT;
  std::vector<PreviewsEligibilityReason> passed_reasons = {
      PreviewsEligibilityReason::PAGE_LOAD_PREDICTION_NOT_PAINFUL,
      PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
      PreviewsEligibilityReason::RELOAD_DISALLOWED,
  };
  const std::vector<PreviewsEligibilityReason> expected_passed_reasons(
      passed_reasons);
  const uint64_t page_id = 1234;
  PreviewsUserData data(page_id);

  previews_decider_impl()->LogPreviewDecisionMade(
      reason, url, time, type, std::move(passed_reasons), &data);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(ui_service()->decision_reasons(), ::testing::ElementsAre(reason));
  EXPECT_THAT(ui_service()->decision_urls(), ::testing::ElementsAre(url));
  EXPECT_THAT(ui_service()->decision_types(), ::testing::ElementsAre(type));
  EXPECT_THAT(ui_service()->decision_times(), ::testing::ElementsAre(time));
  EXPECT_THAT(ui_service()->decision_ids(), ::testing::ElementsAre(page_id));

  EXPECT_EQ(data.EligibilityReasonForPreview(type).value(), reason);
  auto actual_passed_reasons = ui_service()->decision_passed_reasons();
  EXPECT_EQ(1UL, actual_passed_reasons.size());
  EXPECT_EQ(expected_passed_reasons.size(), actual_passed_reasons[0].size());
  for (size_t i = 0; i < actual_passed_reasons[0].size(); i++) {
    EXPECT_EQ(expected_passed_reasons[i], actual_passed_reasons[0][i]);
  }
}

TEST_F(
    PreviewsDeciderImplTest,
    LogDecisionMadeBlocklistUnavailableAtNavigationStartForCommitTimePreview) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kDeferAllScriptPreviews}, {});

  InitializeUIService();
  auto expected_reason = PreviewsEligibilityReason::ALLOWED;
  auto expected_type = PreviewsType::DEFER_ALL_SCRIPT;

  previews_decider_impl()->InjectTestBlocklist(nullptr /* blocklist */);
  PreviewsUserData user_data(kDefaultPageId);
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://www.google.com"));
  previews_decider_impl()->ShouldAllowPreviewAtNavigationStart(
      &user_data, &navigation_handle, false, expected_type);
  base::RunLoop().RunUntilIdle();
  // Testing correct log method is called.
  EXPECT_THAT(ui_service()->decision_reasons(),
              ::testing::Contains(expected_reason));
  EXPECT_THAT(ui_service()->decision_types(),
              ::testing::Contains(expected_type));
}

TEST_F(PreviewsDeciderImplTest, ShouldCommitPreviewBlocklistStatuses) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kDeferAllScriptPreviews}, {});
  InitializeUIService(/*include_previews_opt_guide=*/false);
  auto expected_type = PreviewsType::DEFER_ALL_SCRIPT;
  PreviewsUserData user_data(kDefaultPageId);
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://www.google.com"));
  // First verify URL is allowed for no blocklist status.
  EXPECT_TRUE(previews_decider_impl()->ShouldCommitPreview(
      &user_data, &navigation_handle, expected_type));

  PreviewsEligibilityReason expected_reasons[] = {
      PreviewsEligibilityReason::BLOCKLIST_DATA_NOT_LOADED,
      PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
      PreviewsEligibilityReason::USER_BLOCKLISTED,
      PreviewsEligibilityReason::HOST_BLOCKLISTED,
  };

  const size_t reasons_size = 4;

  for (size_t i = 0; i < reasons_size; i++) {
    auto expected_reason = expected_reasons[i];

    std::unique_ptr<TestPreviewsBlockList> blocklist =
        std::make_unique<TestPreviewsBlockList>(expected_reason,
                                                previews_decider_impl());
    previews_decider_impl()->InjectTestBlocklist(std::move(blocklist));
    PreviewsUserData user_data(kDefaultPageId);
    content::MockNavigationHandle navigation_handle;
    navigation_handle.set_url(GURL("https://www.google.com"));
    EXPECT_FALSE(previews_decider_impl()->ShouldCommitPreview(
        &user_data, &navigation_handle, expected_type));
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

TEST_F(PreviewsDeciderImplTest, LogDecisionMadeMediaSuffixesAreExcluded) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kDeferAllScriptPreviews}, {});
  InitializeUIService();
  auto expected_reason = PreviewsEligibilityReason::EXCLUDED_BY_MEDIA_SUFFIX;
  auto expected_type = PreviewsType::DEFER_ALL_SCRIPT;

  PreviewsEligibilityReason blocklist_decisions[] = {
      PreviewsEligibilityReason::BLOCKLIST_DATA_NOT_LOADED,
  };

  for (auto blocklist_decision : blocklist_decisions) {
    std::unique_ptr<TestPreviewsBlockList> blocklist =
        std::make_unique<TestPreviewsBlockList>(blocklist_decision,
                                                previews_decider_impl());
    previews_decider_impl()->InjectTestBlocklist(std::move(blocklist));
    PreviewsUserData user_data(kDefaultPageId);
    content::MockNavigationHandle navigation_handle;
    navigation_handle.set_url(GURL("https://www.google.com/video.mp4"));
    previews_decider_impl()->ShouldAllowPreviewAtNavigationStart(
        &user_data, &navigation_handle, false, expected_type);

    base::RunLoop().RunUntilIdle();
    // Testing correct log method is called.
    EXPECT_THAT(ui_service()->decision_reasons(),
                ::testing::Contains(expected_reason));
    EXPECT_THAT(ui_service()->decision_types(),
                ::testing::Contains(expected_type));
  }
}

TEST_F(PreviewsDeciderImplTest, IgnoreFlagDoesNotCheckBlocklist) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kDeferAllScriptPreviews}, {});
  InitializeUIService();
  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  previews_decider_impl()->SetIgnorePreviewsBlocklistDecision(
      true /* ignored */);
  PreviewsUserData user_data(kDefaultPageId);
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://allowlisted.example.com"));
  EXPECT_TRUE(previews_decider_impl()->ShouldCommitPreview(
      &user_data, &navigation_handle, PreviewsType::DEFER_ALL_SCRIPT));

  previews_decider_impl()->AddPreviewReload();

  EXPECT_TRUE(previews_decider_impl()->ShouldCommitPreview(
      &user_data, &navigation_handle, PreviewsType::DEFER_ALL_SCRIPT));
}

TEST_F(PreviewsDeciderImplTest, ReloadsTriggerFiveMinuteRule) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kDeferAllScriptPreviews}, {});
  InitializeUIService();
  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  PreviewsUserData user_data(kDefaultPageId);
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://allowlisted.example.com"));
  EXPECT_TRUE(previews_decider_impl()->ShouldCommitPreview(
      &user_data, &navigation_handle, PreviewsType::DEFER_ALL_SCRIPT));

  previews_decider_impl()->AddPreviewNavigation(
      GURL("http://wwww.somedomain.com"), false, PreviewsType::DEFER_ALL_SCRIPT,
      1);

  previews_decider_impl()->AddPreviewReload();

  EXPECT_FALSE(previews_decider_impl()->ShouldCommitPreview(
      &user_data, &navigation_handle, PreviewsType::DEFER_ALL_SCRIPT));
  EXPECT_EQ(PreviewsEligibilityReason::USER_RECENTLY_OPTED_OUT,
            ui_service()->decision_reasons().back());

  clock_.Advance(base::TimeDelta::FromMinutes(6));

  EXPECT_TRUE(previews_decider_impl()->ShouldCommitPreview(
      &user_data, &navigation_handle, PreviewsType::DEFER_ALL_SCRIPT));
}


TEST_F(PreviewsDeciderImplTest, LogDecisionMadeReloadDisallowed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kDeferAllScriptPreviews}, {});
  InitializeUIService();
  std::unique_ptr<TestPreviewsBlockList> blocklist =
      std::make_unique<TestPreviewsBlockList>(
          PreviewsEligibilityReason::ALLOWED, previews_decider_impl());
  previews_decider_impl()->InjectTestBlocklist(std::move(blocklist));

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);
  PreviewsUserData user_data(kDefaultPageId);
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://www.google.com"));

  auto expected_reason = PreviewsEligibilityReason::RELOAD_DISALLOWED;
  auto expected_type = PreviewsType::DEFER_ALL_SCRIPT;

  std::vector<PreviewsEligibilityReason> checked_decisions = {
      PreviewsEligibilityReason::URL_HAS_BASIC_AUTH,
      PreviewsEligibilityReason::EXCLUDED_BY_MEDIA_SUFFIX,
  };

  previews_decider_impl()->ShouldAllowPreviewAtNavigationStart(
      &user_data, &navigation_handle, true, expected_type);
  base::RunLoop().RunUntilIdle();

  // Testing correct log method is called.
  EXPECT_THAT(ui_service()->decision_reasons(),
              ::testing::Contains(expected_reason));
  EXPECT_THAT(ui_service()->decision_types(),
              ::testing::Contains(expected_type));

  EXPECT_EQ(1UL, ui_service()->decision_passed_reasons().size());
  auto actual_passed_reasons = ui_service()->decision_passed_reasons()[0];
  EXPECT_EQ(checked_decisions.size(), actual_passed_reasons.size());
  for (size_t i = 0; i < actual_passed_reasons.size(); i++) {
    EXPECT_EQ(checked_decisions[i], actual_passed_reasons[i]);
  }
}

TEST_F(PreviewsDeciderImplTest, IgnoreBlocklistEnabledViaFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kDeferAllScriptPreviews}, {});
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitch(switches::kIgnorePreviewsBlocklist);
  ASSERT_TRUE(switches::ShouldIgnorePreviewsBlocklist());

  InitializeUIService();

  std::unique_ptr<TestPreviewsBlockList> blocklist =
      std::make_unique<TestPreviewsBlockList>(
          PreviewsEligibilityReason::HOST_BLOCKLISTED, previews_decider_impl());
  previews_decider_impl()->InjectTestBlocklist(std::move(blocklist));
  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);
  PreviewsUserData user_data(kDefaultPageId);
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://allowlisted.example.com"));
  EXPECT_TRUE(previews_decider_impl()->ShouldCommitPreview(
      &user_data, &navigation_handle, PreviewsType::DEFER_ALL_SCRIPT));
}

TEST_F(PreviewsDeciderImplTest, LogDecisionMadeAllowHintPreviewWithoutECT) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kPreviews, features::kDeferAllScriptPreviews}, {});
  InitializeUIService();

  std::unique_ptr<TestPreviewsBlockList> blocklist =
      std::make_unique<TestPreviewsBlockList>(
          PreviewsEligibilityReason::ALLOWED, previews_decider_impl());

  previews_decider_impl()->InjectTestBlocklist(std::move(blocklist));

  ReportEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);

  auto expected_reason = PreviewsEligibilityReason::ALLOWED;
  auto expected_type = PreviewsType::DEFER_ALL_SCRIPT;

  std::vector<PreviewsEligibilityReason> checked_decisions = {
      PreviewsEligibilityReason::URL_HAS_BASIC_AUTH,
      PreviewsEligibilityReason::EXCLUDED_BY_MEDIA_SUFFIX,
      PreviewsEligibilityReason::RELOAD_DISALLOWED,
  };
  PreviewsUserData user_data(kDefaultPageId);
  content::MockNavigationHandle navigation_handle;
  navigation_handle.set_url(GURL("https://allowlisted.example.com"));
  EXPECT_TRUE(previews_decider_impl()->ShouldAllowPreviewAtNavigationStart(
      &user_data, &navigation_handle, false, expected_type));
  base::RunLoop().RunUntilIdle();

  // Testing correct log method is called.
  EXPECT_THAT(ui_service()->decision_reasons(),
              ::testing::Contains(expected_reason));
  EXPECT_THAT(ui_service()->decision_types(),
              ::testing::Contains(expected_type));

  EXPECT_EQ(1UL, ui_service()->decision_passed_reasons().size());
  auto actual_passed_reasons = ui_service()->decision_passed_reasons()[0];
  EXPECT_EQ(checked_decisions.size(), actual_passed_reasons.size());
  for (size_t i = 0; i < actual_passed_reasons.size(); i++) {
    EXPECT_EQ(checked_decisions[i], actual_passed_reasons[i]);
  }
}

TEST_F(PreviewsDeciderImplTest, OnNewBlocklistedHostCallsUIMethodCorrectly) {
  InitializeUIService();
  std::string expected_host = "example.com";
  base::Time expected_time = base::Time::Now();
  previews_decider_impl()->OnNewBlocklistedHost(expected_host, expected_time);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(expected_host, ui_service()->host_blocklisted());
  EXPECT_EQ(expected_time, ui_service()->host_blocklisted_time());
}

TEST_F(PreviewsDeciderImplTest, OnUserBlocklistedCallsUIMethodCorrectly) {
  InitializeUIService();
  previews_decider_impl()->OnUserBlocklistedStatusChange(
      true /* blocklisted */);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ui_service()->user_blocklisted());

  previews_decider_impl()->OnUserBlocklistedStatusChange(
      false /* blocklisted */);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(ui_service()->user_blocklisted());
}

TEST_F(PreviewsDeciderImplTest, OnBlocklistClearedCallsUIMethodCorrectly) {
  InitializeUIService();
  base::Time expected_time = base::Time::Now();
  previews_decider_impl()->OnBlocklistCleared(expected_time);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(expected_time, ui_service()->blocklist_cleared_time());
}

TEST_F(PreviewsDeciderImplTest,
       OnIgnoreBlocklistDecisionStatusChangedCalledCorrect) {
  InitializeUIService();
  previews_decider_impl()->SetIgnorePreviewsBlocklistDecision(
      true /* ignored */);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ui_service()->blocklist_ignored());

  previews_decider_impl()->SetIgnorePreviewsBlocklistDecision(
      false /* ignored */);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(ui_service()->blocklist_ignored());
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

}  // namespace

}  // namespace previews
