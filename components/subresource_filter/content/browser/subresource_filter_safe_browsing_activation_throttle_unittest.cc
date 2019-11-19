// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_activation_throttle.h"

#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/safe_browsing/db/test_database_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/fake_safe_browsing_database_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client.h"
#include "components/subresource_filter/content/browser/subresource_filter_safe_browsing_client_request.h"
#include "components/subresource_filter/content/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_renderer_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

const char kUrlA[] = "https://example_a.com";
const char kUrlB[] = "https://example_b.com";
const char kUrlC[] = "https://example_c.com";

char kURL[] = "http://example.test/";
char kURLWithParams[] = "http://example.test/?v=10";
char kRedirectURL[] = "http://redirect.test/";

const char kSafeBrowsingNavigationDelay[] =
    "SubresourceFilter.PageLoad.SafeBrowsingDelay";
const char kSafeBrowsingCheckTime[] =
    "SubresourceFilter.SafeBrowsing.TotalCheckTime";
const char kActivationListHistogram[] =
    "SubresourceFilter.PageLoad.ActivationList";

class MockSubresourceFilterClient : public SubresourceFilterClient {
 public:
  MockSubresourceFilterClient() = default;
  ~MockSubresourceFilterClient() override = default;

  mojom::ActivationLevel OnPageActivationComputed(
      content::NavigationHandle* handle,
      mojom::ActivationLevel effective_level,
      ActivationDecision* decision) override {
    DCHECK(handle->IsInMainFrame());
    if (whitelisted_hosts_.count(handle->GetURL().host())) {
      if (effective_level ==
          subresource_filter::mojom::ActivationLevel::kEnabled)
        *decision = subresource_filter::ActivationDecision::URL_WHITELISTED;
      return mojom::ActivationLevel::kDisabled;
    }
    return effective_level;
  }

  MOCK_METHOD0(ShowNotification, void());
  MOCK_METHOD0(ForceActivationInCurrentWebContents, bool());

  void WhitelistInCurrentWebContents(const GURL& url) {
    ASSERT_TRUE(url.SchemeIsHTTPOrHTTPS());
    whitelisted_hosts_.insert(url.host());
  }

  void ClearWhitelist() { whitelisted_hosts_.clear(); }

 private:
  std::set<std::string> whitelisted_hosts_;

  DISALLOW_COPY_AND_ASSIGN(MockSubresourceFilterClient);
};

struct ActivationListTestData {
  const char* const activation_list;
  ActivationList activation_list_type;
  safe_browsing::SBThreatType threat_type;
  safe_browsing::ThreatPatternType threat_pattern_type;
  safe_browsing::SubresourceFilterMatch match;
};

typedef safe_browsing::SubresourceFilterLevel SBLevel;
typedef safe_browsing::SubresourceFilterType SBType;
const ActivationListTestData kActivationListTestData[] = {
    {kActivationListSocialEngineeringAdsInterstitial,
     ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS,
     {}},
    {kActivationListPhishingInterstitial,
     ActivationList::PHISHING_INTERSTITIAL,
     safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
     safe_browsing::ThreatPatternType::NONE,
     {}},
    {kActivationListSubresourceFilter,
     ActivationList::SUBRESOURCE_FILTER,
     safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER,
     safe_browsing::ThreatPatternType::NONE,
     {}},
    {kActivationListSubresourceFilter,
     ActivationList::BETTER_ADS,
     safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER,
     safe_browsing::ThreatPatternType::NONE,
     {{SBType::BETTER_ADS, SBLevel::ENFORCE}}},
};

}  //  namespace

class SubresourceFilterSafeBrowsingActivationThrottleTest
    : public content::RenderViewHostTestHarness,
      public content::WebContentsObserver {
 public:
  SubresourceFilterSafeBrowsingActivationThrottleTest() {}

  ~SubresourceFilterSafeBrowsingActivationThrottleTest() override {}

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    Configure();
    test_io_task_runner_ = new base::TestMockTimeTaskRunner();
    // Note: Using NiceMock to allow uninteresting calls and suppress warnings.
    std::vector<url_pattern_index::proto::UrlRule> rules;
    rules.push_back(testing::CreateSuffixRule("disallowed.html"));
    ASSERT_NO_FATAL_FAILURE(test_ruleset_creator_.CreateRulesetWithRules(
        rules, &test_ruleset_pair_));
    ruleset_dealer_ = std::make_unique<VerifiedRulesetDealer::Handle>(
        base::ThreadTaskRunnerHandle::Get());
    ruleset_dealer_->TryOpenAndSetRulesetFile(test_ruleset_pair_.indexed.path,
                                              /*expected_checksum=*/0,
                                              base::DoNothing());

    auto* contents = RenderViewHostTestHarness::web_contents();
    client_ =
        std::make_unique<::testing::NiceMock<MockSubresourceFilterClient>>();
    throttle_manager_ =
        std::make_unique<ContentSubresourceFilterThrottleManager>(
            client_.get(), ruleset_dealer_.get(), contents);
    fake_safe_browsing_database_ = new FakeSafeBrowsingDatabaseManager();
    NavigateAndCommit(GURL("https://test.com"));
    Observe(contents);

    observer_ = std::make_unique<TestSubresourceFilterObserver>(contents);
  }

  virtual void Configure() {
    scoped_configuration_.ResetConfiguration(Configuration(
        mojom::ActivationLevel::kEnabled, ActivationScope::ACTIVATION_LIST,
        ActivationList::SUBRESOURCE_FILTER));
  }

  void TearDown() override {
    ruleset_dealer_.reset();

    // RunUntilIdle() must be called multiple times to flush any outstanding
    // cross-thread interactions.
    // TODO(csharrison): Clean up test teardown logic.
    RunUntilIdle();
    RunUntilIdle();

    // RunUntilIdle() called once more, to delete the database on the IO thread.
    fake_safe_browsing_database_ = nullptr;
    RunUntilIdle();

    content::RenderViewHostTestHarness::TearDown();
  }

  TestSubresourceFilterObserver* observer() { return observer_.get(); }

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsInMainFrame()) {
      navigation_handle->RegisterThrottleForTesting(
          std::make_unique<SubresourceFilterSafeBrowsingActivationThrottle>(
              navigation_handle, client(), test_io_task_runner_,
              fake_safe_browsing_database_));
    }
    std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
    throttle_manager_->MaybeAppendNavigationThrottles(navigation_handle,
                                                      &throttles);
    for (auto& it : throttles) {
      navigation_handle->RegisterThrottleForTesting(std::move(it));
    }
  }

  // Returns the frame host the navigation committed in, or nullptr if it did
  // not succeed.
  content::RenderFrameHost* CreateAndNavigateDisallowedSubframe(
      content::RenderFrameHost* parent) {
    auto* subframe =
        content::RenderFrameHostTester::For(parent)->AppendChild("subframe");
    auto simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL("https://example.test/disallowed.html"), subframe);
    simulator->Commit();
    return simulator->GetLastThrottleCheckResult().action() ==
                   content::NavigationThrottle::PROCEED
               ? simulator->GetFinalRenderFrameHost()
               : nullptr;
  }

  content::RenderFrameHost* SimulateNavigateAndCommit(
      std::vector<GURL> navigation_chain,
      content::RenderFrameHost* rfh) {
    SimulateStart(navigation_chain.front(), rfh);
    for (auto it = navigation_chain.begin() + 1; it != navigation_chain.end();
         ++it) {
      SimulateRedirectAndExpectProceed(*it);
    }
    SimulateCommitAndExpectProceed();
    return navigation_simulator_->GetFinalRenderFrameHost();
  }

  content::NavigationThrottle::ThrottleCheckResult SimulateStart(
      const GURL& first_url,
      content::RenderFrameHost* rfh) {
    navigation_simulator_ =
        content::NavigationSimulator::CreateRendererInitiated(first_url, rfh);
    navigation_simulator_->Start();
    auto result = navigation_simulator_->GetLastThrottleCheckResult();
    if (result.action() == content::NavigationThrottle::CANCEL)
      navigation_simulator_.reset();
    return result;
  }

  content::NavigationThrottle::ThrottleCheckResult SimulateRedirect(
      const GURL& new_url) {
    navigation_simulator_->Redirect(new_url);
    auto result = navigation_simulator_->GetLastThrottleCheckResult();
    if (result.action() == content::NavigationThrottle::CANCEL)
      navigation_simulator_.reset();
    return result;
  }

  content::NavigationThrottle::ThrottleCheckResult SimulateCommit(
      content::NavigationSimulator* simulator) {
    // Need to post a task to flush the IO thread because calling Commit()
    // blocks until the throttle checks are complete.
    // TODO(csharrison): Consider adding finer grained control to the
    // NavigationSimulator by giving it an option to be driven by a
    // TestMockTimeTaskRunner. Also see https://crbug.com/703346.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&base::TestMockTimeTaskRunner::RunUntilIdle,
                       base::Unretained(test_io_task_runner_.get())));
    simulator->Commit();
    return simulator->GetLastThrottleCheckResult();
  }

  void SimulateStartAndExpectProceed(const GURL& first_url) {
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              SimulateStart(first_url, main_rfh()));
  }

  void SimulateRedirectAndExpectProceed(const GURL& new_url) {
    EXPECT_EQ(content::NavigationThrottle::PROCEED, SimulateRedirect(new_url));
  }

  void SimulateCommitAndExpectProceed() {
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              SimulateCommit(navigation_simulator()));
  }

  void ConfigureForMatch(const GURL& url,
                         safe_browsing::SBThreatType pattern_type =
                             safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER,
                         const safe_browsing::ThreatMetadata& metadata =
                             safe_browsing::ThreatMetadata()) {
    fake_safe_browsing_database_->AddBlacklistedUrl(url, pattern_type,
                                                    metadata);
  }

  FakeSafeBrowsingDatabaseManager* fake_safe_browsing_database() {
    return fake_safe_browsing_database_.get();
  }

  void ClearAllBlacklistedUrls() {
    fake_safe_browsing_database_->RemoveAllBlacklistedUrls();
  }

  void RunUntilIdle() {
    base::RunLoop().RunUntilIdle();
    test_io_task_runner_->RunUntilIdle();
  }

  content::NavigationSimulator* navigation_simulator() {
    return navigation_simulator_.get();
  }

  const base::HistogramTester& tester() const { return tester_; }

  MockSubresourceFilterClient* client() { return client_.get(); }

  base::TestMockTimeTaskRunner* test_io_task_runner() const {
    return test_io_task_runner_.get();
  }

  testing::ScopedSubresourceFilterConfigurator* scoped_configuration() {
    return &scoped_configuration_;
  }

 private:
  testing::ScopedSubresourceFilterConfigurator scoped_configuration_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_io_task_runner_;

  testing::TestRulesetCreator test_ruleset_creator_;
  testing::TestRulesetPair test_ruleset_pair_;

  std::unique_ptr<VerifiedRulesetDealer::Handle> ruleset_dealer_;

  std::unique_ptr<ContentSubresourceFilterThrottleManager> throttle_manager_;

  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;
  std::unique_ptr<MockSubresourceFilterClient> client_;
  std::unique_ptr<TestSubresourceFilterObserver> observer_;
  scoped_refptr<FakeSafeBrowsingDatabaseManager> fake_safe_browsing_database_;
  base::HistogramTester tester_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterSafeBrowsingActivationThrottleTest);
};

class SubresourceFilterSafeBrowsingActivationThrottleParamTest
    : public SubresourceFilterSafeBrowsingActivationThrottleTest,
      public ::testing::WithParamInterface<ActivationListTestData> {
 public:
  SubresourceFilterSafeBrowsingActivationThrottleParamTest() {}
  ~SubresourceFilterSafeBrowsingActivationThrottleParamTest() override {}

  void Configure() override {
    const ActivationListTestData& test_data = GetParam();
    scoped_configuration()->ResetConfiguration(Configuration(
        mojom::ActivationLevel::kEnabled, ActivationScope::ACTIVATION_LIST,
        test_data.activation_list_type));
  }

  void ConfigureForMatchParam(const GURL& url) {
    const ActivationListTestData& test_data = GetParam();
    safe_browsing::ThreatMetadata metadata;
    metadata.threat_pattern_type = test_data.threat_pattern_type;
    metadata.subresource_filter_match = test_data.match;
    ConfigureForMatch(url, test_data.threat_type, metadata);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      SubresourceFilterSafeBrowsingActivationThrottleParamTest);
};

class SubresourceFilterSafeBrowsingActivationThrottleTestWithCancelling
    : public SubresourceFilterSafeBrowsingActivationThrottleTest,
      public ::testing::WithParamInterface<
          std::tuple<content::TestNavigationThrottle::ThrottleMethod,
                     content::TestNavigationThrottle::ResultSynchrony>> {
 public:
  SubresourceFilterSafeBrowsingActivationThrottleTestWithCancelling() {
    std::tie(throttle_method_, result_sync_) = GetParam();
  }
  ~SubresourceFilterSafeBrowsingActivationThrottleTestWithCancelling()
      override {}

  void DidStartNavigation(content::NavigationHandle* handle) override {
    auto throttle = std::make_unique<content::TestNavigationThrottle>(handle);
    throttle->SetResponse(throttle_method_, result_sync_,
                          content::NavigationThrottle::CANCEL);
    handle->RegisterThrottleForTesting(std::move(throttle));
    SubresourceFilterSafeBrowsingActivationThrottleTest::DidStartNavigation(
        handle);
  }

  content::TestNavigationThrottle::ThrottleMethod throttle_method() {
    return throttle_method_;
  }

  content::TestNavigationThrottle::ResultSynchrony result_sync() {
    return result_sync_;
  }

 private:
  content::TestNavigationThrottle::ThrottleMethod throttle_method_;
  content::TestNavigationThrottle::ResultSynchrony result_sync_;

  DISALLOW_COPY_AND_ASSIGN(
      SubresourceFilterSafeBrowsingActivationThrottleTestWithCancelling);
};

struct ActivationScopeTestData {
  mojom::ActivationLevel expected_activation_level;
  bool url_matches_activation_list;
  ActivationScope activation_scope;
};

const ActivationScopeTestData kActivationScopeTestData[] = {
    {mojom::ActivationLevel::kEnabled, false /* url_matches_activation_list */,
     ActivationScope::ALL_SITES},
    {mojom::ActivationLevel::kEnabled, true /* url_matches_activation_list */,
     ActivationScope::ALL_SITES},
    {mojom::ActivationLevel::kDisabled, true /* url_matches_activation_list */,
     ActivationScope::NO_SITES},
    {mojom::ActivationLevel::kEnabled, true /* url_matches_activation_list */,
     ActivationScope::ACTIVATION_LIST},
    {mojom::ActivationLevel::kDisabled, false /* url_matches_activation_list */,
     ActivationScope::ACTIVATION_LIST},
};

class SubresourceFilterSafeBrowsingActivationThrottleScopeTest
    : public SubresourceFilterSafeBrowsingActivationThrottleTest,
      public ::testing::WithParamInterface<ActivationScopeTestData> {
 public:
  SubresourceFilterSafeBrowsingActivationThrottleScopeTest() {}
  ~SubresourceFilterSafeBrowsingActivationThrottleScopeTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(
      SubresourceFilterSafeBrowsingActivationThrottleScopeTest);
};

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest, NoConfigs) {
  scoped_configuration()->ResetConfiguration(std::vector<Configuration>());
  SimulateNavigateAndCommit({GURL(kURL)}, main_rfh());
  EXPECT_EQ(mojom::ActivationLevel::kDisabled,
            *observer()->GetPageActivationForLastCommittedLoad());
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       MultipleSimultaneousConfigs) {
  Configuration config1(mojom::ActivationLevel::kDryRun,
                        ActivationScope::NO_SITES);
  config1.activation_conditions.priority = 2;

  Configuration config2(mojom::ActivationLevel::kDisabled,
                        ActivationScope::ACTIVATION_LIST,
                        ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL);
  config2.activation_conditions.priority = 1;

  Configuration config3(mojom::ActivationLevel::kEnabled,
                        ActivationScope::ALL_SITES);
  config3.activation_conditions.priority = 0;

  scoped_configuration()->ResetConfiguration({config1, config2, config3});

  // Should match |config2| and |config3|, the former with the higher priority.
  GURL match_url(kUrlA);
  GURL non_match_url(kUrlB);
  safe_browsing::ThreatMetadata metadata;
  metadata.threat_pattern_type =
      safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS;
  ConfigureForMatch(match_url, safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
                    metadata);
  SimulateNavigateAndCommit({match_url}, main_rfh());
  EXPECT_EQ(mojom::ActivationLevel::kDisabled,
            *observer()->GetPageActivationForLastCommittedLoad());

  // Should match |config3|.
  SimulateNavigateAndCommit({non_match_url}, main_rfh());
  EXPECT_EQ(mojom::ActivationLevel::kEnabled,
            *observer()->GetPageActivationForLastCommittedLoad());
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       ActivationLevelDisabled_NoActivation) {
  scoped_configuration()->ResetConfiguration(Configuration(
      mojom::ActivationLevel::kDisabled, ActivationScope::ACTIVATION_LIST,
      ActivationList::SUBRESOURCE_FILTER));
  GURL url(kURL);

  SimulateNavigateAndCommit({url}, main_rfh());
  EXPECT_EQ(mojom::ActivationLevel::kDisabled,
            *observer()->GetPageActivationForLastCommittedLoad());

  ConfigureForMatch(url);
  SimulateNavigateAndCommit({url}, main_rfh());
  EXPECT_EQ(mojom::ActivationLevel::kDisabled,
            *observer()->GetPageActivationForLastCommittedLoad());

  // Whitelisting occurs last, so the decision should still be DISABLED.
  client()->WhitelistInCurrentWebContents(url);
  SimulateNavigateAndCommit({url}, main_rfh());
  EXPECT_EQ(mojom::ActivationLevel::kDisabled,
            *observer()->GetPageActivationForLastCommittedLoad());
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       AllSiteEnabled_Activates) {
  scoped_configuration()->ResetConfiguration(Configuration(
      mojom::ActivationLevel::kEnabled, ActivationScope::ALL_SITES));
  GURL url(kURL);
  SimulateNavigateAndCommit({url}, main_rfh());
  EXPECT_EQ(mojom::ActivationLevel::kEnabled,
            *observer()->GetPageActivationForLastCommittedLoad());

  ConfigureForMatch(url);
  SimulateNavigateAndCommit({url}, main_rfh());
  EXPECT_EQ(mojom::ActivationLevel::kEnabled,
            *observer()->GetPageActivationForLastCommittedLoad());

  // Adding performance measurement should keep activation.
  Configuration config_with_perf(Configuration(mojom::ActivationLevel::kEnabled,
                                               ActivationScope::ALL_SITES));
  config_with_perf.activation_options.performance_measurement_rate = 1.0;
  scoped_configuration()->ResetConfiguration(std::move(config_with_perf));
  SimulateNavigateAndCommit({url}, main_rfh());
  EXPECT_EQ(mojom::ActivationLevel::kEnabled,
            *observer()->GetPageActivationForLastCommittedLoad());
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       NavigationFails_NoActivation) {
  EXPECT_EQ(base::Optional<mojom::ActivationLevel>(),
            observer()->GetPageActivationForLastCommittedLoad());
  content::NavigationSimulator::NavigateAndFailFromDocument(
      GURL(kURL), net::ERR_TIMED_OUT, main_rfh());
  EXPECT_EQ(base::Optional<mojom::ActivationLevel>(),
            observer()->GetPageActivationForLastCommittedLoad());
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       NotificationVisibility) {
  GURL url(kURL);
  ConfigureForMatch(url);
  content::RenderFrameHost* rfh = SimulateNavigateAndCommit({url}, main_rfh());

  EXPECT_CALL(*client(), ShowNotification()).Times(1);
  EXPECT_FALSE(CreateAndNavigateDisallowedSubframe(rfh));
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest, ActivationList) {
  const struct {
    mojom::ActivationLevel expected_activation_level;
    ActivationList activation_list;
    safe_browsing::SBThreatType threat_type;
    safe_browsing::ThreatPatternType threat_type_metadata;
  } kTestCases[] = {
      {mojom::ActivationLevel::kDisabled, ActivationList::NONE,
       safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {mojom::ActivationLevel::kDisabled,
       ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
       safe_browsing::ThreatPatternType::NONE},
      {mojom::ActivationLevel::kDisabled,
       ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
       safe_browsing::ThreatPatternType::MALWARE_LANDING},
      {mojom::ActivationLevel::kDisabled,
       ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
       safe_browsing::ThreatPatternType::MALWARE_DISTRIBUTION},
      {mojom::ActivationLevel::kDisabled, ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_API_ABUSE,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {mojom::ActivationLevel::kDisabled, ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_BLACKLISTED_RESOURCE,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {mojom::ActivationLevel::kDisabled, ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {mojom::ActivationLevel::kDisabled, ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_BINARY_MALWARE,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {mojom::ActivationLevel::kDisabled, ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_UNWANTED,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {mojom::ActivationLevel::kDisabled, ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_MALWARE,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {mojom::ActivationLevel::kDisabled, ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {mojom::ActivationLevel::kDisabled, ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_SAFE,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {mojom::ActivationLevel::kEnabled, ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
       safe_browsing::ThreatPatternType::NONE},
      {mojom::ActivationLevel::kEnabled,
       ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {mojom::ActivationLevel::kEnabled, ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
       safe_browsing::ThreatPatternType::SOCIAL_ENGINEERING_ADS},
      {mojom::ActivationLevel::kEnabled, ActivationList::SUBRESOURCE_FILTER,
       safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       safe_browsing::ThreatPatternType::NONE},
      {mojom::ActivationLevel::kDisabled, ActivationList::PHISHING_INTERSTITIAL,
       safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER,
       safe_browsing::ThreatPatternType::NONE},
  };
  const GURL test_url("https://matched_url.com/");
  for (const auto& test_case : kTestCases) {
    scoped_configuration()->ResetConfiguration(Configuration(
        mojom::ActivationLevel::kEnabled, ActivationScope::ACTIVATION_LIST,
        test_case.activation_list));
    ClearAllBlacklistedUrls();
    safe_browsing::ThreatMetadata metadata;
    metadata.threat_pattern_type = test_case.threat_type_metadata;
    ConfigureForMatch(test_url, test_case.threat_type, metadata);
    SimulateNavigateAndCommit({GURL(kUrlA), GURL(kUrlB), GURL(kUrlC), test_url},
                              main_rfh());
    EXPECT_EQ(test_case.expected_activation_level,
              *observer()->GetPageActivationForLastCommittedLoad());
  }
}

// Regression test for an issue where synchronous failure from the SB database
// caused a double cancel. This is DCHECKed in the fake database.
TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       SynchronousResponse) {
  const GURL url(kURL);
  fake_safe_browsing_database()->set_synchronous_failure();
  SimulateStartAndExpectProceed(url);
  SimulateCommitAndExpectProceed();
  tester().ExpectUniqueSample(kActivationListHistogram, ActivationList::NONE,
                              1);
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest, LogsUkm) {
  ukm::InitializeSourceUrlRecorderForWebContents(
      RenderViewHostTestHarness::web_contents());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  const GURL url(kURL);
  ConfigureForMatch(url);
  SimulateNavigateAndCommit({url}, main_rfh());
  using SubresourceFilter = ukm::builders::SubresourceFilter;
  const auto& entries =
      test_ukm_recorder.GetEntriesByName(SubresourceFilter::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    test_ukm_recorder.ExpectEntrySourceHasUrl(entry, url);
    test_ukm_recorder.ExpectEntryMetric(
        entry, SubresourceFilter::kActivationDecisionName,
        static_cast<int64_t>(ActivationDecision::ACTIVATED));
  }
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       LogsUkmNoActivation) {
  ukm::InitializeSourceUrlRecorderForWebContents(
      RenderViewHostTestHarness::web_contents());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  const GURL url(kURL);
  SimulateNavigateAndCommit({url}, main_rfh());
  using SubresourceFilter = ukm::builders::SubresourceFilter;
  const auto& entries =
      test_ukm_recorder.GetEntriesByName(SubresourceFilter::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    test_ukm_recorder.ExpectEntrySourceHasUrl(entry, url);
    test_ukm_recorder.ExpectEntryMetric(
        entry, SubresourceFilter::kActivationDecisionName,
        static_cast<int64_t>(
            ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET));
  }
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest, LogsUkmDryRun) {
  scoped_configuration()->ResetConfiguration(Configuration(
      mojom::ActivationLevel::kDryRun, ActivationScope::ALL_SITES));
  ukm::InitializeSourceUrlRecorderForWebContents(
      RenderViewHostTestHarness::web_contents());
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  const GURL url(kURL);
  SimulateNavigateAndCommit({url}, main_rfh());
  using SubresourceFilter = ukm::builders::SubresourceFilter;
  const auto& entries =
      test_ukm_recorder.GetEntriesByName(SubresourceFilter::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    test_ukm_recorder.ExpectEntrySourceHasUrl(entry, url);
    test_ukm_recorder.ExpectEntryMetric(
        entry, SubresourceFilter::kActivationDecisionName,
        static_cast<int64_t>(ActivationDecision::ACTIVATED));
    test_ukm_recorder.ExpectEntryMetric(entry, SubresourceFilter::kDryRunName,
                                        true);
  }
}

TEST_P(SubresourceFilterSafeBrowsingActivationThrottleScopeTest,
       ActivateForScopeType) {
  const ActivationScopeTestData& test_data = GetParam();
  scoped_configuration()->ResetConfiguration(Configuration(
      mojom::ActivationLevel::kEnabled, test_data.activation_scope,
      ActivationList::SUBRESOURCE_FILTER));

  const GURL test_url(kURLWithParams);
  if (test_data.url_matches_activation_list)
    ConfigureForMatch(test_url);
  SimulateNavigateAndCommit({test_url}, main_rfh());
  EXPECT_EQ(test_data.expected_activation_level,
            *observer()->GetPageActivationForLastCommittedLoad());
  if (test_data.url_matches_activation_list) {
    client()->WhitelistInCurrentWebContents(test_url);
    SimulateNavigateAndCommit({test_url}, main_rfh());
    EXPECT_EQ(mojom::ActivationLevel::kDisabled,
              *observer()->GetPageActivationForLastCommittedLoad());
  }
}

// Only main frames with http/https schemes should activate.
TEST_P(SubresourceFilterSafeBrowsingActivationThrottleScopeTest,
       ActivateForSupportedUrlScheme) {
  const ActivationScopeTestData& test_data = GetParam();
  scoped_configuration()->ResetConfiguration(Configuration(
      mojom::ActivationLevel::kEnabled, test_data.activation_scope,
      ActivationList::SUBRESOURCE_FILTER));

  // data URLs are also not supported, but not listed here, as it's not possible
  // for a page to redirect to them after https://crbug.com/594215 is fixed.
  const char* unsupported_urls[] = {"ftp://example.com/", "chrome://settings",
                                    "chrome-extension://some-extension",
                                    "file:///var/www/index.html"};
  const char* supported_urls[] = {"http://example.test",
                                  "https://example.test"};
  for (auto* url : unsupported_urls) {
    SCOPED_TRACE(url);
    if (test_data.url_matches_activation_list)
      ConfigureForMatch(GURL(url));
    SimulateNavigateAndCommit({GURL(url)}, main_rfh());
    EXPECT_EQ(mojom::ActivationLevel::kDisabled,
              *observer()->GetPageActivationForLastCommittedLoad());
  }

  for (auto* url : supported_urls) {
    SCOPED_TRACE(url);
    if (test_data.url_matches_activation_list)
      ConfigureForMatch(GURL(url));
    SimulateNavigateAndCommit({GURL(url)}, main_rfh());
    EXPECT_EQ(test_data.expected_activation_level,
              *observer()->GetPageActivationForLastCommittedLoad());
  }
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       ListNotMatched_NoActivation) {
  const GURL url(kURL);
  SimulateStartAndExpectProceed(url);
  SimulateCommitAndExpectProceed();
  EXPECT_EQ(mojom::ActivationLevel::kDisabled,
            *observer()->GetPageActivationForLastCommittedLoad());
  tester().ExpectUniqueSample(kActivationListHistogram,
                              static_cast<int>(ActivationList::NONE), 1);

  tester().ExpectTotalCount(kSafeBrowsingNavigationDelay, 1);
  tester().ExpectTotalCount(kSafeBrowsingCheckTime, 1);
}

TEST_P(SubresourceFilterSafeBrowsingActivationThrottleParamTest,
       ListMatched_Activation) {
  const ActivationListTestData& test_data = GetParam();
  const GURL url(kURL);
  ConfigureForMatchParam(url);
  SimulateStartAndExpectProceed(url);
  SimulateCommitAndExpectProceed();
  EXPECT_EQ(mojom::ActivationLevel::kEnabled,
            *observer()->GetPageActivationForLastCommittedLoad());
  tester().ExpectUniqueSample(kActivationListHistogram,
                              static_cast<int>(test_data.activation_list_type),
                              1);
}

TEST_P(SubresourceFilterSafeBrowsingActivationThrottleParamTest,
       ListNotMatchedAfterRedirect_NoActivation) {
  const GURL url(kURL);
  ConfigureForMatchParam(url);
  SimulateStartAndExpectProceed(url);
  SimulateRedirectAndExpectProceed(GURL(kRedirectURL));
  SimulateCommitAndExpectProceed();
  EXPECT_EQ(mojom::ActivationLevel::kDisabled,
            *observer()->GetPageActivationForLastCommittedLoad());
  tester().ExpectUniqueSample(kActivationListHistogram,
                              static_cast<int>(ActivationList::NONE), 1);
}

TEST_P(SubresourceFilterSafeBrowsingActivationThrottleParamTest,
       ListMatchedAfterRedirect_Activation) {
  const ActivationListTestData& test_data = GetParam();
  const GURL url(kURL);
  ConfigureForMatchParam(GURL(kRedirectURL));
  SimulateStartAndExpectProceed(url);
  SimulateRedirectAndExpectProceed(GURL(kRedirectURL));
  SimulateCommitAndExpectProceed();
  EXPECT_EQ(mojom::ActivationLevel::kEnabled,
            *observer()->GetPageActivationForLastCommittedLoad());
  tester().ExpectUniqueSample(kActivationListHistogram,
                              static_cast<int>(test_data.activation_list_type),
                              1);
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       ListNotMatchedAndTimeout_NoActivation) {
  const GURL url(kURL);
  fake_safe_browsing_database()->SimulateTimeout();
  SimulateStartAndExpectProceed(url);

  // Flush the pending tasks on the IO thread, so the delayed task surely gets
  // posted.
  test_io_task_runner()->RunUntilIdle();

  // Expect one delayed task, and fast forward time.
  base::TimeDelta expected_delay =
      SubresourceFilterSafeBrowsingClientRequest::kCheckURLTimeout;
  EXPECT_EQ(expected_delay, test_io_task_runner()->NextPendingTaskDelay());
  test_io_task_runner()->FastForwardBy(expected_delay);
  SimulateCommitAndExpectProceed();
  EXPECT_EQ(mojom::ActivationLevel::kDisabled,
            *observer()->GetPageActivationForLastCommittedLoad());
  tester().ExpectTotalCount(kSafeBrowsingNavigationDelay, 1);
  tester().ExpectTotalCount(kSafeBrowsingCheckTime, 1);
}

// Flaky on Win, Chromium and Linux. http://crbug.com/748524
TEST_P(SubresourceFilterSafeBrowsingActivationThrottleParamTest,
       DISABLED_ListMatchedOnStart_NoDelay) {
  const ActivationListTestData& test_data = GetParam();
  const GURL url(kURL);
  ConfigureForMatchParam(url);
  SimulateStartAndExpectProceed(url);

  // Get the database result back before commit.
  RunUntilIdle();

  SimulateCommitAndExpectProceed();
  EXPECT_EQ(mojom::ActivationLevel::kEnabled,
            *observer()->GetPageActivationForLastCommittedLoad());
  tester().ExpectUniqueSample(kActivationListHistogram,
                              static_cast<int>(test_data.activation_list_type),
                              1);

  tester().ExpectTimeBucketCount(kSafeBrowsingNavigationDelay,
                                 base::TimeDelta::FromMilliseconds(0), 1);
}

// Flaky on Win, Chromium and Linux. http://crbug.com/748524
TEST_P(SubresourceFilterSafeBrowsingActivationThrottleParamTest,
       DISABLED_ListMatchedOnRedirect_NoDelay) {
  const ActivationListTestData& test_data = GetParam();
  const GURL url(kURL);
  const GURL redirect_url(kRedirectURL);
  ConfigureForMatchParam(redirect_url);

  SimulateStartAndExpectProceed(url);
  SimulateRedirectAndExpectProceed(redirect_url);

  // Get the database result back before commit.
  RunUntilIdle();

  SimulateCommitAndExpectProceed();
  EXPECT_EQ(mojom::ActivationLevel::kEnabled,
            *observer()->GetPageActivationForLastCommittedLoad());
  tester().ExpectUniqueSample(kActivationListHistogram,
                              static_cast<int>(test_data.activation_list_type),
                              1);

  tester().ExpectTimeBucketCount(kSafeBrowsingNavigationDelay,
                                 base::TimeDelta::FromMilliseconds(0), 1);
  tester().ExpectTotalCount(kSafeBrowsingCheckTime, 2);
}

struct RedirectSamplesAndResults {
  std::vector<GURL> urls;
  bool expected_activation;
  base::Optional<RedirectPosition> last_enforcement_position;
};

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       ActivationTriggeredOnRedirect) {
  // Turn on the feature to perform safebrowsing on redirects.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kSafeBrowsingSubresourceFilterConsiderRedirects);
  std::string histogram_string =
      "SubresourceFilter.PageLoad.Activation.RedirectPosition2.Enforcement";

  // Set up the urls for enforcement.
  GURL normal_url("https://example.regular");
  GURL bad_url("https://example.bad");
  GURL worse_url("https://example.worse");

  // Set up the configurations, make phishing worse than subresource_filter.
  Configuration config_p1(mojom::ActivationLevel::kEnabled,
                          ActivationScope::ACTIVATION_LIST,
                          ActivationList::SUBRESOURCE_FILTER);
  config_p1.activation_conditions.priority = 1;
  Configuration config_p2(mojom::ActivationLevel::kEnabled,
                          ActivationScope::ACTIVATION_LIST,
                          ActivationList::PHISHING_INTERSTITIAL);
  config_p2.activation_conditions.priority = 2;
  scoped_configuration()->ResetConfiguration({config_p1, config_p2});

  // Configure the URLs to match on different lists, phishing is worse.
  ConfigureForMatch(bad_url, safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER);
  ConfigureForMatch(worse_url, safe_browsing::SB_THREAT_TYPE_URL_PHISHING);

  // Check cases where there are multiple redirection.
  const RedirectSamplesAndResults kTestCases[] = {
      {{worse_url, normal_url, normal_url}, true, RedirectPosition::kFirst},
      {{bad_url, normal_url, worse_url}, true, RedirectPosition::kLast},
      {{worse_url, normal_url, bad_url}, true, RedirectPosition::kLast},
      {{normal_url, worse_url, bad_url}, true, RedirectPosition::kLast},
      {{normal_url, normal_url}, false, base::nullopt},
      {{normal_url, bad_url, normal_url}, false, RedirectPosition::kMiddle},
      {{worse_url}, true, RedirectPosition::kOnly},
  };
  for (const auto& test_case : kTestCases) {
    const base::HistogramTester histograms;
    SimulateStartAndExpectProceed(test_case.urls[0]);
    for (size_t index = 1; index < test_case.urls.size(); index++) {
      SimulateRedirectAndExpectProceed(test_case.urls[index]);
    }
    RunUntilIdle();
    SimulateCommitAndExpectProceed();
    if (test_case.expected_activation) {
      EXPECT_EQ(mojom::ActivationLevel::kEnabled,
                *observer()->GetPageActivationForLastCommittedLoad());
    } else {
      EXPECT_EQ(mojom::ActivationLevel::kDisabled,
                *observer()->GetPageActivationForLastCommittedLoad());
    }
    if (test_case.last_enforcement_position.has_value()) {
      histograms.ExpectUniqueSample(histogram_string,
                                    *test_case.last_enforcement_position, 1);
    } else {
      histograms.ExpectTotalCount(histogram_string, 0);
    }
  }
}

TEST_F(SubresourceFilterSafeBrowsingActivationThrottleTest,
       ActivationTriggeredOnAbusiveSites) {
  for (bool enable_adblock_on_abusive_sites : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "enable_adblock_on_abusive_sites = "
                                      << enable_adblock_on_abusive_sites);
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureState(
        subresource_filter::kFilterAdsOnAbusiveSites,
        enable_adblock_on_abusive_sites);
    scoped_configuration()->ResetConfiguration(
        Configuration::MakePresetForLiveRunForBetterAds());

    const GURL url(kURL);
    safe_browsing::ThreatMetadata metadata;
    metadata.threat_pattern_type = safe_browsing::ThreatPatternType::NONE;
    metadata.subresource_filter_match = safe_browsing::SubresourceFilterMatch(
        {{SBType::ABUSIVE, SBLevel::ENFORCE}});
    ConfigureForMatch(url, safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER,
                      metadata);

    SimulateStartAndExpectProceed(url);
    SimulateCommitAndExpectProceed();
    EXPECT_EQ(enable_adblock_on_abusive_sites
                  ? mojom::ActivationLevel::kEnabled
                  : mojom::ActivationLevel::kDisabled,
              *observer()->GetPageActivationForLastCommittedLoad());
  }
}

// Disabled due to flaky failures: https://crbug.com/753669.
TEST_P(SubresourceFilterSafeBrowsingActivationThrottleParamTest,
       DISABLED_ListMatchedOnStartWithRedirect_NoActivation) {
  const GURL url(kURL);
  const GURL redirect_url(kRedirectURL);
  ConfigureForMatchParam(url);

  // These two lines also test how the database client reacts to two requests
  // happening one after another.
  SimulateStartAndExpectProceed(url);
  SimulateRedirectAndExpectProceed(redirect_url);

  // Get the database result back before commit.
  RunUntilIdle();

  SimulateCommitAndExpectProceed();
  EXPECT_EQ(mojom::ActivationLevel::kDisabled,
            *observer()->GetPageActivationForLastCommittedLoad());
  tester().ExpectTimeBucketCount(kSafeBrowsingNavigationDelay,
                                 base::TimeDelta::FromMilliseconds(0), 1);
}

TEST_P(SubresourceFilterSafeBrowsingActivationThrottleTestWithCancelling,
       Cancel) {
  const GURL url(kURL);
  SCOPED_TRACE(::testing::Message() << "ThrottleMethod: " << throttle_method()
                                    << " ResultSynchrony: " << result_sync());
  ConfigureForMatch(url);
  content::NavigationThrottle::ThrottleCheckResult result =
      SimulateStart(url, main_rfh());
  if (throttle_method() ==
      content::TestNavigationThrottle::WILL_START_REQUEST) {
    EXPECT_EQ(content::NavigationThrottle::CANCEL, result);
    tester().ExpectTotalCount(kSafeBrowsingNavigationDelay, 0);
    return;
  }
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result);

  result = SimulateRedirect(GURL(kRedirectURL));
  if (throttle_method() ==
      content::TestNavigationThrottle::WILL_REDIRECT_REQUEST) {
    EXPECT_EQ(content::NavigationThrottle::CANCEL, result);
    tester().ExpectTotalCount(kSafeBrowsingNavigationDelay, 0);
    return;
  }
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result);

  base::RunLoop().RunUntilIdle();

  result = SimulateCommit(navigation_simulator());
  EXPECT_EQ(content::NavigationThrottle::CANCEL, result);
  tester().ExpectTotalCount(kSafeBrowsingNavigationDelay, 0);
}

INSTANTIATE_TEST_SUITE_P(
    CancelMethod,
    SubresourceFilterSafeBrowsingActivationThrottleTestWithCancelling,
    ::testing::Combine(
        ::testing::Values(
            content::TestNavigationThrottle::WILL_START_REQUEST,
            content::TestNavigationThrottle::WILL_REDIRECT_REQUEST,
            content::TestNavigationThrottle::WILL_PROCESS_RESPONSE),
        ::testing::Values(content::TestNavigationThrottle::SYNCHRONOUS,
                          content::TestNavigationThrottle::ASYNCHRONOUS)));

INSTANTIATE_TEST_SUITE_P(
    ActivationLevelTest,
    SubresourceFilterSafeBrowsingActivationThrottleParamTest,
    ::testing::ValuesIn(kActivationListTestData));

INSTANTIATE_TEST_SUITE_P(
    ActivationScopeTest,
    SubresourceFilterSafeBrowsingActivationThrottleScopeTest,
    ::testing::ValuesIn(kActivationScopeTestData));

}  // namespace subresource_filter
