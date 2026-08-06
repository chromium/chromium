// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_settings_impl.h"

#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "base/json/values_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "base/values.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace privacy_sandbox {
namespace {

using ::privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate;
using ::privacy_sandbox_test_util::MultipleInputKeys;
using ::privacy_sandbox_test_util::MultipleOutputKeys;
using ::privacy_sandbox_test_util::MultipleStateKeys;
using ::privacy_sandbox_test_util::PrivacySandboxSettingsTestPeer;
using ::privacy_sandbox_test_util::SiteDataExceptions;
using ::privacy_sandbox_test_util::TestCase;
using ::privacy_sandbox_test_util::TestInput;
using ::privacy_sandbox_test_util::TestOutput;
using ::privacy_sandbox_test_util::TestState;
using ::testing::Return;

using Status = PrivacySandboxSettingsTestPeer::Status;

using enum privacy_sandbox_test_util::StateKey;
using enum privacy_sandbox_test_util::InputKey;
using enum privacy_sandbox_test_util::OutputKey;

constexpr int kTestTaxonomyVersion = 1;

}  // namespace

class PrivacySandboxSettingsTest : public testing::Test {
 public:
  PrivacySandboxSettingsTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scoped_attestations_(PrivacySandboxAttestations::CreateForTesting()) {
    // Mark all Privacy Sandbox APIs as attested since the test cases are
    // testing behaviors not related to attestations.
    PrivacySandboxAttestations::GetInstance()
        ->SetAllPrivacySandboxAttestedForTesting(true);
    content_settings::CookieSettings::RegisterProfilePrefs(prefs()->registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs()->registry());
    RegisterProfilePrefs(prefs()->registry());
    host_content_settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */, false /* should_record_metrics */);
    cookie_settings_ = new content_settings::CookieSettings(
        host_content_settings_map_.get(), &prefs_, false,
        content_settings::CookieSettings::NoFedCmSharingPermissionsCallback(),
        "chrome-extension");
  }
  ~PrivacySandboxSettingsTest() override {
    cookie_settings()->ShutdownOnUIThread();
    host_content_settings_map()->ShutdownOnUIThread();
  }

  void SetUp() override {
    auto mock_delegate = std::make_unique<
        testing::NiceMock<MockPrivacySandboxSettingsDelegate>>();
    mock_delegate_ = mock_delegate.get();

    InitializePrefsBeforeStart();
    InitializeFeaturesBeforeStart();
    InitializeDelegateBeforeStart();

    privacy_sandbox_settings_ = std::make_unique<PrivacySandboxSettingsImpl>(
        std::move(mock_delegate), host_content_settings_map(), cookie_settings_,
        prefs());
  }

  virtual void InitializePrefsBeforeStart() {}

  virtual void InitializeFeaturesBeforeStart() {}

  virtual void InitializeDelegateBeforeStart() {
    mock_delegate()->SetUpIsPrivacySandboxRestrictedResponse(
        /*restricted=*/false);
    mock_delegate()->SetUpIsIncognitoProfileResponse(/*incognito=*/false);
  }

  MockPrivacySandboxSettingsDelegate* mock_delegate() { return mock_delegate_; }
  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }
  HostContentSettingsMap* host_content_settings_map() {
    return host_content_settings_map_.get();
  }
  content_settings::CookieSettings* cookie_settings() {
    return cookie_settings_.get();
  }
  PrivacySandboxSettings* privacy_sandbox_settings() {
    return privacy_sandbox_settings_.get();
  }
  PrivacySandboxSettingsImpl* privacy_sandbox_settings_impl() {
    return privacy_sandbox_settings_.get();
  }
  bool IsFledgeJoiningAllowed(const std::string& url) {
    return PrivacySandboxSettingsTestPeer(privacy_sandbox_settings_impl())
        .IsFledgeJoiningAllowed(url::Origin::Create(GURL(url)));
  }
  content::BrowserTaskEnvironment* task_environment() {
    return &browser_task_environment_;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList disabled_topics_feature_list_;

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  raw_ptr<MockPrivacySandboxSettingsDelegate, DanglingUntriaged> mock_delegate_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  ScopedPrivacySandboxAttestations scoped_attestations_;

  std::unique_ptr<PrivacySandboxSettingsImpl> privacy_sandbox_settings_;
};

TEST_F(PrivacySandboxSettingsTest, TopicsDataAccessibleSince) {
  ASSERT_NE(base::Time(), base::Time::Now());

  EXPECT_EQ(base::Time(),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());

  privacy_sandbox_settings()->OnCookiesCleared();

  EXPECT_EQ(base::Time::Now(),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());
}

TEST_F(PrivacySandboxSettingsTest, FledgeJoiningAllowed) {
  // Whether or not a site can join a user to an interest group is independent
  // of any other profile state.
  EXPECT_TRUE(IsFledgeJoiningAllowed("https://example.com"));

  // Settings should match at the eTLD + 1 level.
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("example.com", false);

  EXPECT_FALSE(IsFledgeJoiningAllowed("https://subsite.example.com"));
  EXPECT_FALSE(IsFledgeJoiningAllowed("http://example.com"));
  EXPECT_FALSE(IsFledgeJoiningAllowed("https://example.com:888"));
  EXPECT_FALSE(IsFledgeJoiningAllowed("https://example.com"));
  EXPECT_TRUE(IsFledgeJoiningAllowed("https://example.com.au"));

  privacy_sandbox_settings()->SetFledgeJoiningAllowed("example.com", true);

  EXPECT_TRUE(IsFledgeJoiningAllowed("https://example.com"));
  EXPECT_TRUE(IsFledgeJoiningAllowed("https://subsite.example.com"));
  EXPECT_TRUE(IsFledgeJoiningAllowed("http://example.com"));
  EXPECT_TRUE(IsFledgeJoiningAllowed("https://example.com:888"));
  EXPECT_TRUE(IsFledgeJoiningAllowed("https://example.com"));
  EXPECT_TRUE(IsFledgeJoiningAllowed("https://example.com.au"));
}

TEST_F(PrivacySandboxSettingsTest, NonEtldPlusOneBlocked) {
  // Confirm that, as a fallback, hosts are accepted by SetFledgeJoiningAllowed.
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("subsite.example.com",
                                                      false);

  // Applied setting should affect subdomaings.
  EXPECT_FALSE(IsFledgeJoiningAllowed("https://subsite.example.com"));
  EXPECT_FALSE(IsFledgeJoiningAllowed("http://another.subsite.example.com"));
  EXPECT_TRUE(IsFledgeJoiningAllowed("https://example.com"));

  // When removing the setting, only an exact match, and not the associated
  // eTLD+1, should remove a setting.
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("example.com", true);
  EXPECT_FALSE(IsFledgeJoiningAllowed("https://subsite.example.com"));
  EXPECT_FALSE(IsFledgeJoiningAllowed("http://another.subsite.example.com"));
  EXPECT_TRUE(IsFledgeJoiningAllowed("https://example.com"));

  privacy_sandbox_settings()->SetFledgeJoiningAllowed("subsite.example.com",
                                                      true);
  EXPECT_TRUE(IsFledgeJoiningAllowed("https://subsite.example.com"));
  EXPECT_TRUE(IsFledgeJoiningAllowed("http://another.subsite.example.com"));
  EXPECT_TRUE(IsFledgeJoiningAllowed("https://example.com"));

  // IP addresses should also be accepted as a fallback.
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("10.1.1.100", false);
  EXPECT_FALSE(IsFledgeJoiningAllowed("https://10.1.1.100"));
  EXPECT_FALSE(IsFledgeJoiningAllowed("http://10.1.1.100:8080"));
  EXPECT_TRUE(IsFledgeJoiningAllowed("https://10.2.2.200"));
}

TEST_F(PrivacySandboxSettingsTest, FledgeJoinSettingTimeRangeDeletion) {
  // Confirm that time range deletions work appropriately for FLEDGE join
  // settings.
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("first.com", false);
  task_environment()->AdvanceClock(base::Hours(1));

  const base::Time kSecondSettingTime = base::Time::Now();
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("second.com", false);

  task_environment()->AdvanceClock(base::Hours(1));
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("third.com", false);

  EXPECT_FALSE(IsFledgeJoiningAllowed("https://first.com"));
  EXPECT_FALSE(IsFledgeJoiningAllowed("https://second.com"));
  EXPECT_FALSE(IsFledgeJoiningAllowed("https://third.com"));

  // Construct a deletion which only targets the second setting.
  privacy_sandbox_settings()->ClearFledgeJoiningAllowedSettings(
      kSecondSettingTime - base::Seconds(1),
      kSecondSettingTime + base::Seconds(1));
  EXPECT_FALSE(IsFledgeJoiningAllowed("https://first.com"));
  EXPECT_TRUE(IsFledgeJoiningAllowed("https://second.com"));
  EXPECT_FALSE(IsFledgeJoiningAllowed("https://third.com"));

  // Perform a maximmal time range deletion, which should remove the two
  // remaining settings.
  privacy_sandbox_settings()->ClearFledgeJoiningAllowedSettings(
      base::Time(), base::Time::Max());
  EXPECT_TRUE(IsFledgeJoiningAllowed("https://first.com"));
  EXPECT_TRUE(IsFledgeJoiningAllowed("https://second.com"));
  EXPECT_TRUE(IsFledgeJoiningAllowed("https://third.com"));
}

TEST_F(PrivacySandboxSettingsTest, OnRelatedWebsiteSetsEnabledChanged) {
  // OnRelatedWebsiteSetsEnabledChanged() should only call observers when the
  // pref changes.
  privacy_sandbox_test_util::MockPrivacySandboxObserver observer;
  privacy_sandbox_settings()->AddObserver(&observer);
  EXPECT_CALL(observer, OnRelatedWebsiteSetsEnabledChanged(/*enabled=*/true));

  prefs()->SetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnRelatedWebsiteSetsEnabledChanged(/*enabled=*/false));
  prefs()->SetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled, false);
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(PrivacySandboxSettingsTest, ClearingTopicSettings) {
  // Confirm that time range deletions affect the correct settings.
  CanonicalTopic topic_a(1, kTestTaxonomyVersion);
  CanonicalTopic topic_b(57, kTestTaxonomyVersion);
  CanonicalTopic topic_c(86, kTestTaxonomyVersion);

  privacy_sandbox_settings()->SetTopicAllowed(topic_a, false);
  task_environment()->AdvanceClock(base::Hours(1));

  const auto kSecondSettingTime = base::Time::Now();
  privacy_sandbox_settings()->SetTopicAllowed(topic_b, false);

  task_environment()->AdvanceClock(base::Hours(1));
  privacy_sandbox_settings()->SetTopicAllowed(topic_c, false);

  EXPECT_EQ(3u, prefs()->GetList(prefs::kPrivacySandboxBlockedTopics).size());

  // Construct a deletion which only targets the second setting.
  privacy_sandbox_settings()->ClearTopicSettings(
      kSecondSettingTime - base::Seconds(1),
      kSecondSettingTime + base::Seconds(1));

  EXPECT_EQ(2u, prefs()->GetList(prefs::kPrivacySandboxBlockedTopics).size());

  // Perform a maximmal time range deletion, which should remove the two
  // remaining settings.
  privacy_sandbox_settings()->ClearTopicSettings(base::Time(),
                                                 base::Time::Max());
  EXPECT_EQ(0u, prefs()->GetList(prefs::kPrivacySandboxBlockedTopics).size());
}

struct PrivateAggregationDebugModeTestCase {
  using TupleT = std::tuple<bool, bool>;

  explicit PrivateAggregationDebugModeTestCase(TupleT t)
      : site_exception_user_setting_defined(std::get<0>(t)),
        ignore_site_exception_feature_enabled(std::get<1>(t)) {}

  bool site_exception_user_setting_defined = false;
  bool ignore_site_exception_feature_enabled = false;
};

// This test class relies on setting the cookie controls mode pref, which is not
// used on iOS.
#if !BUILDFLAG(IS_IOS)
class PrivacySandboxSettingsPrivateAggregationDebugModeTest
    : public PrivacySandboxSettingsTest,
      public testing::WithParamInterface<PrivateAggregationDebugModeTestCase> {
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PrivacySandboxSettingsPrivateAggregationDebugModeTest,
    testing::ConvertGenerator<PrivateAggregationDebugModeTestCase::TupleT>(
        testing::Combine(testing::Bool(),
                         testing::Bool())),
    // Creates a human-readable name for each test. Per gtest docs, test names
    // must contain only alphanumeric characters.
    [](const testing::TestParamInfo<PrivateAggregationDebugModeTestCase>& info)
        -> std::string {
      return base::StringPrintf(
          "AndSiteExceptionUserSetting%s"
          "AndIgnoreSiteException%s",
          info.param.site_exception_user_setting_defined ? "Defined"
                                                         : "NotDefined",
          info.param.ignore_site_exception_feature_enabled ? "On" : "Off");
    });

// Test that Private Aggregation Debug Mode can be enabled in some circumstances
// even though third-party cookies are blocked.
TEST_P(PrivacySandboxSettingsPrivateAggregationDebugModeTest,
       IsPrivateAggregationDebugModeAllowed) {
  // Debug Mode should be disabled when third-party cookies are blocked,
  // unless they are allowed due to an explicit user site exception.
  //
  // Additionally, if third-party cookies are re-enabled with a top-level site
  // exception, that will allow for debug mode unless the ignore site exception
  // feature is enabled.
  const PrivateAggregationDebugModeTestCase& test_case = GetParam();

  base::test::ScopedFeatureList feature_list;
  std::vector<base::test::FeatureRef> enabled_features, disabled_features = {};
  if (test_case.ignore_site_exception_feature_enabled) {
    enabled_features.emplace_back(
        kPrivateAggregationDebugReportingIgnoreSiteExceptions);
  } else {
    disabled_features.emplace_back(
        kPrivateAggregationDebugReportingIgnoreSiteExceptions);
  }
  feature_list.InitWithFeatures(enabled_features, disabled_features);

  // Enable ad measurement pref. Otherwise, Private Aggregation will not be
  // allowed by PrivacySandboxSettingsImpl::IsPrivateAggregationAllowed().
  prefs()->SetUserPref(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                       base::Value(true));

  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  if (test_case.site_exception_user_setting_defined) {
    host_content_settings_map()->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString("https://embedded.com"),
        ContentSettingsPattern::FromString("https://test.com"),
        ContentSettingsType::COOKIES, ContentSetting::CONTENT_SETTING_ALLOW);
  }

  EXPECT_FALSE(privacy_sandbox_settings()->IsPrivateAggregationDebugModeAllowed(
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://embedded.com"))));
}
#endif

class PrivacySandboxSettingsTestCookiesClearOnExitTurnedOff
    : public PrivacySandboxSettingsTest {
 public:
  void InitializePrefsBeforeStart() override {
    prefs()->SetUserPref(prefs::kPrivacySandboxTopicsDataAccessibleSince,
                         std::make_unique<base::Value>(::base::TimeToValue(
                             base::Time::FromTimeT(12345))));
  }
};

TEST_F(PrivacySandboxSettingsTestCookiesClearOnExitTurnedOff,
       UseLastTopicsDataAccessibleSince) {
  EXPECT_EQ(base::Time::FromTimeT(12345),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());
}

class PrivacySandboxSettingsTestCookiesClearOnExitTurnedOn
    : public PrivacySandboxSettingsTest {
 public:
  void InitializePrefsBeforeStart() override {
    host_content_settings_map()->SetDefaultContentSetting(
        ContentSettingsType::COOKIES,
        ContentSetting::CONTENT_SETTING_SESSION_ONLY);

    prefs()->SetUserPref(prefs::kPrivacySandboxTopicsDataAccessibleSince,
                         std::make_unique<base::Value>(::base::TimeToValue(
                             base::Time::FromTimeT(12345))));
  }
};

TEST_F(PrivacySandboxSettingsTestCookiesClearOnExitTurnedOn,
       UpdateTopicsDataAccessibleSince) {
  EXPECT_EQ(base::Time::Now(),
            privacy_sandbox_settings()->TopicsDataAccessibleSince());
}

class PrivacySandboxSettingsMockDelegateTest
    : public PrivacySandboxSettingsTest {
 public:
  void InitializeDelegateBeforeStart() override {
    // Do not set default handlers so each call must be mocked.
  }
};

// Tests class for the PrivacySandboxSettings4 / M1 launch.
class PrivacySandboxSettingsM1Test : public PrivacySandboxSettingsTest {
 protected:
  void RunTestCase(const TestState& test_state,
                   const TestInput& test_input,
                   const TestOutput& test_output) {
    ASSERT_FALSE(test_case_run_)
        << "Each test fixture should run a single test, to ensure the test "
           "profile is in a known state.";
    test_case_run_ = true;
    auto user_provider = std::make_unique<content_settings::MockProvider>();
    auto* user_provider_raw = user_provider.get();
    auto managed_provider = std::make_unique<content_settings::MockProvider>();
    auto* managed_provider_raw = managed_provider.get();

    content_settings::TestUtils::OverrideProvider(
        host_content_settings_map(), std::move(user_provider),
        content_settings::ProviderType::kPrefProvider);
    content_settings::TestUtils::OverrideProvider(
        host_content_settings_map(), std::move(managed_provider),
        content_settings::ProviderType::kPolicyProvider);

    privacy_sandbox_test_util::RunTestCase(
        task_environment(), prefs(), host_content_settings_map(),
        mock_delegate(), privacy_sandbox_settings(), nullptr, user_provider_raw,
        managed_provider_raw, TestCase(test_state, test_input, test_output));
  }

 protected:
  // Pseudo-constants for the convenience of tests that need to check values of
  // type bool*. We can't actually make these const, as then dereferencing them
  // would give the wrong type (i.e. const bool*). Since we are using
  // std::variant for the test outputs, the types must exactly match.
  bool kTrue_ = true;
  bool kFalse_ = false;

  bool actual_out_shared_storage_block_is_site_setting_specific_ = false;
  bool actual_out_select_url_block_is_site_setting_specific_ = false;
  bool actual_out_private_aggregation_block_is_site_setting_specific_ = false;

 private:
  bool test_case_run_ = false;
};

class PrivacySandboxAttestationsTest : public base::test::WithFeatureOverride,
                                       public PrivacySandboxSettingsM1Test {
 public:
  PrivacySandboxAttestationsTest()
      : base::test::WithFeatureOverride(
            kDefaultAllowPrivacySandboxAttestations) {
    // This test suite tests Privacy Sandbox Attestations related behaviors,
    // turn off the setting that makes all APIs considered attested.
    PrivacySandboxAttestations::GetInstance()
        ->SetAllPrivacySandboxAttestedForTesting(false);
  }

  bool IsAttestationsDefaultAllowed() const { return IsParamFeatureEnabled(); }
};

// When the browser hasn't yet confirmed that the attestations file is present
// in the filesystem. An attestation check:
// 1. succeeds if `kDefaultAllowPrivacySandboxAttestations` is on.
// 2. fails otherwise.
TEST_P(PrivacySandboxAttestationsTest, AttestationsFileNotYetChecked) {
  GURL top_frame_url("https://top-frame.com");
  GURL enrollee_url("https://embedded.com");
  RunTestCase(
      TestState{
          {MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                             kM1FledgeEnabledUserPrefValue,
                             kM1AdMeasurementEnabledUserPrefValue},
           true},
          {kAttestationsMap, std::nullopt},
      },
      TestInput{
          {kTopicsURL, enrollee_url},
          {kTopFrameOrigin, url::Origin::Create(top_frame_url)},
          {kAdMeasurementReportingOrigin, url::Origin::Create(enrollee_url)},
          {kFledgeAuctionPartyOrigin, url::Origin::Create(enrollee_url)},
          {kEventReportingDestinationOrigin, url::Origin::Create(enrollee_url)},
          {kAccessingOrigin, url::Origin::Create(enrollee_url)},

          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_},
          {kOutPrivateAggregationBlockIsSiteSettingSpecific,
           &actual_out_private_aggregation_block_is_site_setting_specific_}},
      TestOutput{
          {MultipleOutputKeys{
               kIsEventReportingDestinationAttestedForFledge,
               kIsEventReportingDestinationAttestedForSharedStorage,
               kIsSharedStorageAllowed},
           IsAttestationsDefaultAllowed()},
          {MultipleOutputKeys{
               kIsSharedStorageAllowedMetric,
               kIsEventReportingDestinationAttestedForSharedStorageMetric,
               kIsEventReportingDestinationAttestedForFledgeMetric},
           static_cast<int>(IsAttestationsDefaultAllowed()
                                ? Status::kAllowed
                                : Status::kAttestationsFileNotYetChecked)},
          {MultipleOutputKeys{kIsSharedStorageBlockSiteSettingSpecific,
                              kIsPrivateAggregationBlockSiteSettingSpecific},
           &kFalse_}});
}

// When the attestations map has no enrollments at all (i.e., no enrollment
// for the site in question), attestation fails.
TEST_P(PrivacySandboxAttestationsTest, NoEnrollments) {
  GURL top_frame_url("https://top-frame.com");
  GURL enrollee_url("https://embedded.com");
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue},
                 true},
                {kAttestationsMap, PrivacySandboxAttestationsMap{}}},
      TestInput{
          {kTopicsURL, enrollee_url},
          {kTopFrameOrigin, url::Origin::Create(top_frame_url)},
          {kAdMeasurementReportingOrigin, url::Origin::Create(enrollee_url)},
          {kFledgeAuctionPartyOrigin, url::Origin::Create(enrollee_url)},
          {kEventReportingDestinationOrigin, url::Origin::Create(enrollee_url)},
          {kAccessingOrigin, url::Origin::Create(enrollee_url)},

          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_},
          {kOutPrivateAggregationBlockIsSiteSettingSpecific,
           &actual_out_private_aggregation_block_is_site_setting_specific_}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowedForContext, kIsFledgeJoinAllowed,
               kIsFledgeLeaveAllowed, kIsFledgeUpdateAllowed,
               kIsFledgeSellAllowed, kIsFledgeBuyAllowed,
               kIsEventReportingDestinationAttestedForFledge,
               kIsEventReportingDestinationAttestedForSharedStorage,
               kIsSharedStorageAllowed, kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed},
           false},
          {MultipleOutputKeys{
               kIsSharedStorageAllowedMetric,
               kIsEventReportingDestinationAttestedForSharedStorageMetric,
               kIsEventReportingDestinationAttestedForFledgeMetric},
           static_cast<int>(Status::kAttestationFailed)},
          {MultipleOutputKeys{kIsSharedStorageBlockSiteSettingSpecific,
                              kIsPrivateAggregationBlockSiteSettingSpecific},
           &kFalse_}});
}

// When the site in question is enrolled but has no attestations at all (i.e.,
// no attestation for the API in question), attestation fails.
TEST_P(PrivacySandboxAttestationsTest, EnrollmentWithoutAttestations) {
  GURL top_frame_url("https://top-frame.com");
  GURL enrollee_url("https://embedded.com");
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue},
                 true},
                {kAttestationsMap,
                 PrivacySandboxAttestationsMap{
                     {net::SchemefulSite(enrollee_url), {}}}}},
      TestInput{
          {kTopicsURL, enrollee_url},
          {kTopFrameOrigin, url::Origin::Create(top_frame_url)},
          {kAdMeasurementReportingOrigin, url::Origin::Create(enrollee_url)},
          {kFledgeAuctionPartyOrigin, url::Origin::Create(enrollee_url)},
          {kEventReportingDestinationOrigin, url::Origin::Create(enrollee_url)},
          {kAccessingOrigin, url::Origin::Create(enrollee_url)}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowedForContext, kIsFledgeJoinAllowed,
               kIsFledgeLeaveAllowed, kIsFledgeUpdateAllowed,
               kIsFledgeSellAllowed, kIsFledgeBuyAllowed,
               kIsEventReportingDestinationAttestedForFledge,
               kIsEventReportingDestinationAttestedForSharedStorage,
               kIsSharedStorageAllowed, kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed},
           false},
          {MultipleOutputKeys{
               kIsSharedStorageAllowedMetric,
               kIsEventReportingDestinationAttestedForSharedStorageMetric,
               kIsEventReportingDestinationAttestedForFledgeMetric},
           static_cast<int>(Status::kAttestationFailed)}});
}

TEST_P(PrivacySandboxAttestationsTest, SetOverrideFromDevtools) {
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));
  privacy_sandbox_settings()->SetAllPrivacySandboxAllowedForTesting();

  // Set an empty attestations map to prevent the API being default allowed
  // when feature `kDefaultAllowPrivacySandboxAttestations` is on.
  PrivacySandboxAttestations::GetInstance()->SetAttestationsForTesting(
      PrivacySandboxAttestationsMap{});

  GURL top_level_url("https://top-level-origin.com");
  GURL caller_url("https://embedded.com");

  // With an empty attestation map, Topics is not allowed.
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      url::Origin::Create(top_level_url), caller_url));
  EXPECT_FALSE(privacy_sandbox_settings()->IsEventReportingDestinationAttested(
      url::Origin::Create(GURL("https://embedded.com")),
      PrivacySandboxAttestationsGatedAPI::kProtectedAudience));

  // With an override of the site from a devtools call, attestation passes, but
  // Topics is disabled.
  PrivacySandboxAttestations::GetInstance()->AddOverride(
      net::SchemefulSite(GURL("https://embedded.com")));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      url::Origin::Create(top_level_url), caller_url));
  EXPECT_TRUE(privacy_sandbox_settings()->IsEventReportingDestinationAttested(
      url::Origin::Create(GURL("https://embedded.com")),
      PrivacySandboxAttestationsGatedAPI::kProtectedAudience));
}

TEST_P(PrivacySandboxAttestationsTest, SetOverrideFromFlags) {
  static const struct TestCase {
    std::string name;
    std::string flags;
    GURL report_url;
    bool expected;
  } kTestCases[] = {
      {"Basic", "https://embedded.com", GURL("https://embedded.com"), true},
      {"Empty", "", GURL("https://embedded.com"), false},
      {"Different", "https://other.com", GURL("https://embedded.com"), false},
      {"Multiple", "https://other.com, https://embedded.com",
       GURL("https://embedded.com"), true},
      {"Invalid", "embedded.com", GURL("https://embedded.com"), false},
      {"Extra Comma", "https://a.com,,https://embedded.com",
       GURL("https://embedded.com"), true},
      {"www", "https://www.embedded.com", GURL("https://embedded.com"), true},
  };
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));
  privacy_sandbox_settings()->SetAllPrivacySandboxAllowedForTesting();
  base::test::ScopedCommandLine scoped_command_line;

  // Set an empty attestations map to prevent the API being default allowed
  // when feature `kDefaultAllowPrivacySandboxAttestations` is on.
  PrivacySandboxAttestations::GetInstance()->SetAttestationsForTesting(
      PrivacySandboxAttestationsMap{});

  for (const auto& test : kTestCases) {
    // Reset the overrides flags from the previous test loop.
    scoped_command_line.GetProcessCommandLine()->RemoveSwitch(
        kPrivacySandboxEnrollmentOverrides);

    // Event reporting for Protected Audience should not be allowed at first.
    EXPECT_FALSE(
        privacy_sandbox_settings()->IsEventReportingDestinationAttested(
            url::Origin::Create(test.report_url),
            PrivacySandboxAttestationsGatedAPI::kProtectedAudience));

    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        kPrivacySandboxEnrollmentOverrides, test.flags);

    // Check reporting for Protected Audience after setting the flag.
    EXPECT_EQ(privacy_sandbox_settings()->IsEventReportingDestinationAttested(
                  url::Origin::Create(test.report_url),
                  PrivacySandboxAttestationsGatedAPI::kProtectedAudience),
              test.expected)
        << test.name;
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PrivacySandboxAttestationsTest);

namespace {

constexpr char kAttestationFailedTemplate[] =
    "Attestation check for Shared Storage on %s failed.\nReturned status %d; "
    "see `PrivacySandboxSettingsImpl::Status` at "
    "https://chromium.googlesource.com/chromium/src/+/refs/heads/main/"
    "components/privacy_sandbox/privacy_sandbox_settings_impl.h.";

class PrivacySandboxSettingsSharedStorageDebugTest
    : public PrivacySandboxSettingsM1Test {
 public:
  PrivacySandboxSettingsSharedStorageDebugTest() {
    default_allow_attestations_feature_list_.InitAndEnableFeature(
        kDefaultAllowPrivacySandboxAttestations);

    // This test suite tests Privacy Sandbox Attestations related behaviors,
    // turn off the setting that makes all APIs considered attested.
    PrivacySandboxAttestations::GetInstance()
        ->SetAllPrivacySandboxAttestedForTesting(false);
  }

 protected:
  std::string actual_out_shared_storage_debug_message_;
  std::string actual_out_select_url_debug_message_;

 private:
  base::test::ScopedFeatureList default_allow_attestations_feature_list_;
};

}  // namespace

TEST_F(PrivacySandboxSettingsSharedStorageDebugTest, NoEnrollments) {
  std::string expected_out_shared_storage_debug_message =
      base::StringPrintf(kAttestationFailedTemplate, "https://embedded.com",
                         static_cast<int>(Status::kAttestationFailed));

  // Confirm that the expected debug message is received when attestation fails
  // due to no enrollments.
  RunTestCase(
      TestState{{kM1FledgeEnabledUserPrefValue, true},
                {kAttestationsMap, PrivacySandboxAttestationsMap{}}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kAccessingOrigin, url::Origin::Create(GURL("https://embedded.com"))},
          {kOutSharedStorageDebugMessage,
           &actual_out_shared_storage_debug_message_},

          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_}},
      TestOutput{{kIsSharedStorageAllowed, false},
                 {kIsSharedStorageAllowedMetric,
                  static_cast<int>(Status::kAttestationFailed)},
                 {kIsSharedStorageAllowedDebugMessage,
                  &expected_out_shared_storage_debug_message},
                 {kIsSharedStorageBlockSiteSettingSpecific, &kFalse_}});
}

}  // namespace privacy_sandbox
