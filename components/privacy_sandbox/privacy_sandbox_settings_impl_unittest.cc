// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_settings_impl.h"

#include <string>
#include <tuple>
#include <vector>

#include "base/json/values_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "base/values.h"
#include "components/browsing_topics/test_util.h"
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
#include "components/privacy_sandbox/tpcd_experiment_eligibility.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace privacy_sandbox {

using Topic = browsing_topics::Topic;

namespace {

using enum privacy_sandbox_test_util::StateKey;
using enum privacy_sandbox_test_util::InputKey;
using enum privacy_sandbox_test_util::OutputKey;

// using enum ContentSetting;
constexpr auto CONTENT_SETTING_ALLOW = ContentSetting::CONTENT_SETTING_ALLOW;
constexpr auto CONTENT_SETTING_BLOCK = ContentSetting::CONTENT_SETTING_BLOCK;

// using enum content_settings::CookieControlsMode;
constexpr auto kBlockThirdParty =
    content_settings::CookieControlsMode::kBlockThirdParty;
constexpr auto kLimitedThirdParty =
    content_settings::CookieControlsMode::kLimited;

constexpr int kTestTaxonomyVersion = 1;

using privacy_sandbox_test_util::MultipleInputKeys;
using privacy_sandbox_test_util::MultipleOutputKeys;
using privacy_sandbox_test_util::MultipleStateKeys;
using privacy_sandbox_test_util::SiteDataExceptions;
using privacy_sandbox_test_util::TestCase;
using privacy_sandbox_test_util::TestInput;
using privacy_sandbox_test_util::TestOutput;
using privacy_sandbox_test_util::TestState;

}  // namespace

class PrivacySandboxSettingsTest : public testing::Test {
 public:
  PrivacySandboxSettingsTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scoped_attestations_(
            privacy_sandbox::PrivacySandboxAttestations::CreateForTesting()) {
    // Mark all Privacy Sandbox APIs as attested since the test cases are
    // testing behaviors not related to attestations.
    privacy_sandbox::PrivacySandboxAttestations::GetInstance()
        ->SetAllPrivacySandboxAttestedForTesting(true);
    content_settings::CookieSettings::RegisterProfilePrefs(prefs()->registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs()->registry());
    privacy_sandbox::RegisterProfilePrefs(prefs()->registry());
    host_content_settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */, false /* should_record_metrics */);
    tracking_protection_settings_ =
        std::make_unique<privacy_sandbox::TrackingProtectionSettings>(
            &prefs_, host_content_settings_map_.get(),
            /*is_incognito=*/false);
    cookie_settings_ = new content_settings::CookieSettings(
        host_content_settings_map_.get(), &prefs_,
        tracking_protection_settings_.get(), false,
        content_settings::CookieSettings::NoFedCmSharingPermissionsCallback(),
        /*tpcd_metadata_manager=*/nullptr, "chrome-extension");
  }
  ~PrivacySandboxSettingsTest() override {
    cookie_settings()->ShutdownOnUIThread();
    host_content_settings_map()->ShutdownOnUIThread();
    tracking_protection_settings_->Shutdown();
  }

  void SetUp() override {
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate_ = mock_delegate.get();

    InitializePrefsBeforeStart();
    InitializeFeaturesBeforeStart();
    InitializeDelegateBeforeStart();

    privacy_sandbox_settings_ = std::make_unique<PrivacySandboxSettingsImpl>(
        std::move(mock_delegate), host_content_settings_map(), cookie_settings_,
        tracking_protection_settings_.get(), prefs());
  }

  virtual void InitializePrefsBeforeStart() {}

  virtual void InitializeFeaturesBeforeStart() {}

  virtual void InitializeDelegateBeforeStart() {
    mock_delegate()->SetUpIsPrivacySandboxRestrictedResponse(
        /*restricted=*/false);
    mock_delegate()->SetUpIsIncognitoProfileResponse(/*incognito=*/false);
    mock_delegate()->SetUpHasAppropriateTopicsConsentResponse(
        /*has_appropriate_consent=*/true);
    mock_delegate()->SetUpIsCookieDeprecationExperimentEligibleResponse(
        /*eligible=*/true);
    mock_delegate()->SetUpGetCookieDeprecationExperimentCurrentEligibility(
        /*eligibility_reason=*/TpcdExperimentEligibility::Reason::kEligible);
    mock_delegate()->SetUpIsCookieDeprecationLabelAllowedResponse(
        /*allowed=*/true);
    mock_delegate()
        ->SetUpAreThirdPartyCookiesBlockedByCookieDeprecationExperimentResponse(
            /*result=*/false);
  }

  privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate*
  mock_delegate() {
    return mock_delegate_;
  }
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
  void ResetDisabledTopicsFeature(const std::string& topics_to_disable) {
    // Recreate the service to reset the cache of topics blocked via Finch.
    auto mock_delegate = std::make_unique<testing::NiceMock<
        privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate>>();
    mock_delegate_ = mock_delegate.get();
    privacy_sandbox_settings_ = std::make_unique<PrivacySandboxSettingsImpl>(
        std::move(mock_delegate), host_content_settings_map(), cookie_settings_,
        tracking_protection_settings_.get(), prefs());

    disabled_topics_feature_list_.Reset();
    disabled_topics_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kBrowsingTopicsParameters,
        {{"disabled_topics_list", topics_to_disable}});
  }
  content::BrowserTaskEnvironment* task_environment() {
    return &browser_task_environment_;
  }
  browsing_topics::MockBrowsingTopicsService* mock_browsing_topics_service() {
    return &mock_browsing_topics_service_;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList disabled_topics_feature_list_;

  using Status = PrivacySandboxSettingsImpl::Status;

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  raw_ptr<privacy_sandbox_test_util::MockPrivacySandboxSettingsDelegate,
          DanglingUntriaged>
      mock_delegate_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  browsing_topics::MockBrowsingTopicsService mock_browsing_topics_service_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  std::unique_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;
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
  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com"))));

  // Settings should match at the eTLD + 1 level.
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("example.com", false);

  EXPECT_FALSE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://subsite.example.com"))));
  EXPECT_FALSE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("http://example.com"))));
  EXPECT_FALSE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com:888"))));
  EXPECT_FALSE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com.au"))));

  privacy_sandbox_settings()->SetFledgeJoiningAllowed("example.com", true);

  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://subsite.example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("http://example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com:888"))));
  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com.au"))));
}

TEST_F(PrivacySandboxSettingsTest, NonEtldPlusOneBlocked) {
  // Confirm that, as a fallback, hosts are accepted by SetFledgeJoiningAllowed.
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("subsite.example.com",
                                                      false);

  // Applied setting should affect subdomaings.
  EXPECT_FALSE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://subsite.example.com"))));
  EXPECT_FALSE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("http://another.subsite.example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com"))));

  // When removing the setting, only an exact match, and not the associated
  // eTLD+1, should remove a setting.
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("example.com", true);
  EXPECT_FALSE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://subsite.example.com"))));
  EXPECT_FALSE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("http://another.subsite.example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com"))));

  privacy_sandbox_settings()->SetFledgeJoiningAllowed("subsite.example.com",
                                                      true);
  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://subsite.example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("http://another.subsite.example.com"))));
  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://example.com"))));

  // IP addresses should also be accepted as a fallback.
  privacy_sandbox_settings()->SetFledgeJoiningAllowed("10.1.1.100", false);
  EXPECT_FALSE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://10.1.1.100"))));
  EXPECT_FALSE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("http://10.1.1.100:8080"))));
  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://10.2.2.200"))));
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

  EXPECT_FALSE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://first.com"))));
  EXPECT_FALSE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://second.com"))));
  EXPECT_FALSE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://third.com"))));

  // Construct a deletion which only targets the second setting.
  privacy_sandbox_settings()->ClearFledgeJoiningAllowedSettings(
      kSecondSettingTime - base::Seconds(1),
      kSecondSettingTime + base::Seconds(1));
  EXPECT_FALSE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://first.com"))));
  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://second.com"))));
  EXPECT_FALSE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://third.com"))));

  // Perform a maximmal time range deletion, which should remove the two
  // remaining settings.
  privacy_sandbox_settings()->ClearFledgeJoiningAllowedSettings(
      base::Time(), base::Time::Max());
  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://first.com"))));
  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://second.com"))));
  EXPECT_TRUE(privacy_sandbox_settings_impl()->IsFledgeJoiningAllowed(
      url::Origin::Create(GURL("https://third.com"))));
}

TEST_F(PrivacySandboxSettingsTest, OnRelatedWebsiteSetsEnabledChanged) {
  // OnRelatedWebsiteSetsEnabledChanged() should only call observers when the
  // pref changes.
  privacy_sandbox_test_util::MockPrivacySandboxObserver observer;
  privacy_sandbox_settings()->AddObserver(&observer);
  EXPECT_CALL(observer, OnFirstPartySetsEnabledChanged(/*enabled=*/true));

  prefs()->SetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnFirstPartySetsEnabledChanged(/*enabled=*/false));
  prefs()->SetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled, false);
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(PrivacySandboxSettingsTest, OnFirstPartySetsEnabledChanged3pcd) {
  privacy_sandbox_test_util::MockPrivacySandboxObserver observer;
  privacy_sandbox_settings()->AddObserver(&observer);

  EXPECT_CALL(observer, OnFirstPartySetsEnabledChanged(/*enabled=*/false));
  prefs()->SetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled, false);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnFirstPartySetsEnabledChanged(/*enabled=*/true));
  prefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnFirstPartySetsEnabledChanged(/*enabled=*/false));
  prefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, false);
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(PrivacySandboxSettingsTest, IsTopicAllowed) {
  // Confirm that allowing / blocking topics is correctly reflected by
  // IsTopicsAllowed().
  CanonicalTopic topic(Topic(1), kTestTaxonomyVersion);
  CanonicalTopic child_topic(Topic(7), kTestTaxonomyVersion);
  CanonicalTopic grandchild_topic(Topic(8), kTestTaxonomyVersion);

  CanonicalTopic unrelated_topic(Topic(57), kTestTaxonomyVersion);

  // Check that a topic and its descendants get blocked.
  privacy_sandbox_settings()->SetTopicAllowed(topic, false);
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(child_topic));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(grandchild_topic));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(unrelated_topic));

  // Check that explicitly blocking an implicitly blocked topic works.
  privacy_sandbox_settings()->SetTopicAllowed(child_topic, false);
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(child_topic));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(grandchild_topic));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(unrelated_topic));

  // Check that a topic remains blocked if its parent is blocked even if the
  // topic is set allowed.
  privacy_sandbox_settings()->SetTopicAllowed(child_topic, true);
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(child_topic));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(grandchild_topic));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(unrelated_topic));

  // Check that unblocking an ancestor unblocks a topic as long as it wasn't
  // explicitly blocked or implicitly blocked by another ancestor.
  privacy_sandbox_settings()->SetTopicAllowed(topic, true);
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(child_topic));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(grandchild_topic));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(unrelated_topic));

  // Check that blocking a descendant doesn't block an ancestor.
  privacy_sandbox_settings()->SetTopicAllowed(child_topic, false);
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(child_topic));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(grandchild_topic));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(unrelated_topic));

  // Check that blocking and unblocking an ancestor doesn't unblock an
  // explicitly blocked descendant or a descendant implicitly blocked by another
  // ancestor.
  privacy_sandbox_settings()->SetTopicAllowed(topic, false);
  privacy_sandbox_settings()->SetTopicAllowed(topic, true);
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(child_topic));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(grandchild_topic));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(unrelated_topic));

  // Check that blocking an unrelated topic doesn't affect our topic or its
  // descendants.
  privacy_sandbox_settings()->SetTopicAllowed(unrelated_topic, false);
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(child_topic));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(grandchild_topic));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(unrelated_topic));
}

TEST_F(PrivacySandboxSettingsTest, IsTopicAllowed_ByFinchSettings) {
  // Confirm that blocking topics in Finch is correctly reflected by
  // IsTopicAllowed().
  CanonicalTopic topic(Topic(1), kTestTaxonomyVersion);
  CanonicalTopic child_topic(Topic(7), kTestTaxonomyVersion);

  // Check that not setting the Finch setting does not cause an error or block a
  // topic.
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic));

  // Check that setting an empty list does not cause an error or block a topic.
  ResetDisabledTopicsFeature("");
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic));

  // Check that blocking a topic does not block its parent.
  ResetDisabledTopicsFeature("7");
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(child_topic));

  // Check that blocking a parent topic blocks the child topic.
  ResetDisabledTopicsFeature("1");
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(child_topic));

  // Try blocking a list of topics.
  ResetDisabledTopicsFeature("1,9,44,330");
  for (int topic_id : {1, 9, 44, 330}) {
    CanonicalTopic canonical_topic =
        CanonicalTopic(Topic(topic_id), kTestTaxonomyVersion);
    EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(canonical_topic));
  }

  // Try blocking a list of topics with extra whitespace.
  ResetDisabledTopicsFeature(" 1  , 9,44, 330  ");
  for (int topic_id : {1, 9, 44, 330}) {
    CanonicalTopic canonical_topic =
        CanonicalTopic(Topic(topic_id), kTestTaxonomyVersion);
    EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(canonical_topic));
  }

  // Try blocking a list of topics where some aren't real topics.
  ResetDisabledTopicsFeature(" 0,1,9,44,330,2920");
  for (int topic_id : {1, 9, 44, 330}) {
    CanonicalTopic canonical_topic =
        CanonicalTopic(Topic(topic_id), kTestTaxonomyVersion);
    EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(canonical_topic));
  }

  // Try blocking an invalid string. It should cause a CHECK to fail.
  ResetDisabledTopicsFeature("Arts");
  EXPECT_CHECK_DEATH(privacy_sandbox_settings()->IsTopicAllowed(topic));
}

TEST_F(PrivacySandboxSettingsTest, ClearingTopicSettings) {
  // Confirm that time range deletions affect the correct settings.
  CanonicalTopic topic_a(Topic(1), kTestTaxonomyVersion);
  CanonicalTopic topic_b(Topic(57), kTestTaxonomyVersion);
  CanonicalTopic topic_c(Topic(86), kTestTaxonomyVersion);
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_a));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_b));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_c));

  privacy_sandbox_settings()->SetTopicAllowed(topic_a, false);
  task_environment()->AdvanceClock(base::Hours(1));

  const auto kSecondSettingTime = base::Time::Now();
  privacy_sandbox_settings()->SetTopicAllowed(topic_b, false);

  task_environment()->AdvanceClock(base::Hours(1));
  privacy_sandbox_settings()->SetTopicAllowed(topic_c, false);

  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic_a));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic_b));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic_c));

  // Construct a deletion which only targets the second setting.
  privacy_sandbox_settings()->ClearTopicSettings(
      kSecondSettingTime - base::Seconds(1),
      kSecondSettingTime + base::Seconds(1));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic_a));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_b));
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicAllowed(topic_c));

  // Perform a maximmal time range deletion, which should remove the two
  // remaining settings.
  privacy_sandbox_settings()->ClearTopicSettings(base::Time(),
                                                 base::Time::Max());
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_a));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_b));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicAllowed(topic_c));
}

TEST_F(PrivacySandboxSettingsTest,
       GetCookieDeprecationExperimentCurrentEligibility) {
  EXPECT_CALL(*mock_delegate(),
              GetCookieDeprecationExperimentCurrentEligibility())
      .Times(1)
      .WillOnce(testing::Return(TpcdExperimentEligibility(
          TpcdExperimentEligibility::Reason::k3pCookiesBlocked)));
  EXPECT_EQ(privacy_sandbox_settings()
                ->GetCookieDeprecationExperimentCurrentEligibility()
                .reason(),
            TpcdExperimentEligibility::Reason::k3pCookiesBlocked);

  EXPECT_CALL(*mock_delegate(),
              GetCookieDeprecationExperimentCurrentEligibility())
      .Times(1)
      .WillOnce(testing::Return(TpcdExperimentEligibility(
          TpcdExperimentEligibility::Reason::kEligible)));
  EXPECT_EQ(privacy_sandbox_settings()
                ->GetCookieDeprecationExperimentCurrentEligibility()
                .reason(),
            TpcdExperimentEligibility::Reason::kEligible);
}

TEST_F(PrivacySandboxSettingsTest, IsCookieDeprecationLabelAllowed) {
  EXPECT_CALL(*mock_delegate(), IsCookieDeprecationLabelAllowed())
      .Times(1)
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(privacy_sandbox_settings()->IsCookieDeprecationLabelAllowed());

  EXPECT_CALL(*mock_delegate(), IsCookieDeprecationLabelAllowed())
      .Times(1)
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(privacy_sandbox_settings()->IsCookieDeprecationLabelAllowed());
}

class PrivacySandboxSettingsAttributionReportingTransitionalDebugModeTest
    : public PrivacySandboxSettingsTest,
      public testing::WithParamInterface<bool> {
 public:
  PrivacySandboxSettingsAttributionReportingTransitionalDebugModeTest() =
      default;
  bool IsAttributionDebugReportingCookieDeprecationTestingEnabled() {
    return GetParam();
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PrivacySandboxSettingsAttributionReportingTransitionalDebugModeTest,
    testing::Bool());

TEST_P(
    PrivacySandboxSettingsAttributionReportingTransitionalDebugModeTest,
    IsAttributionReportingTransitionalDebuggingAllowed_CanBypassInCookieDeprecationExperiment) {
  bool enabled = IsAttributionDebugReportingCookieDeprecationTestingEnabled();
  SCOPED_TRACE(enabled);

  base::test::ScopedFeatureList feature_list_;
  if (enabled) {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{content_settings::features::kTrackingProtection3pcd, {}},
         {kAttributionDebugReportingCookieDeprecationTesting, {}}},
        /*disabled_features=*/{});
  } else {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{content_settings::features::
                                   kTrackingProtection3pcd,
                               {}}},
        /*disabled_features=*/{
            kAttributionDebugReportingCookieDeprecationTesting});
  }

  bool ara_transitional_debug_reporting_can_bypass = false;

  if (enabled) {
    EXPECT_CALL(*mock_delegate(),
                AreThirdPartyCookiesBlockedByCookieDeprecationExperiment())
        .WillOnce(testing::Return(false));
  } else {
    EXPECT_CALL(*mock_delegate(),
                AreThirdPartyCookiesBlockedByCookieDeprecationExperiment())
        .Times(0);
  }

  // Disallowed not due to cookie deprecation experiment, therefore cannot
  // bypass.
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));
  EXPECT_FALSE(privacy_sandbox_settings()
                   ->IsAttributionReportingTransitionalDebuggingAllowed(
                       url::Origin::Create(GURL("https://test.com")),
                       url::Origin::Create(GURL("https://embedded.com")),
                       ara_transitional_debug_reporting_can_bypass));
  EXPECT_FALSE(ara_transitional_debug_reporting_can_bypass);

  if (enabled) {
    EXPECT_CALL(*mock_delegate(),
                AreThirdPartyCookiesBlockedByCookieDeprecationExperiment())
        .WillOnce(testing::Return(true));
  } else {
    EXPECT_CALL(*mock_delegate(),
                AreThirdPartyCookiesBlockedByCookieDeprecationExperiment())
        .Times(0);
  }

  // Disallowed due to cookie deprecation experiment, therefore can bypass
  // when feature enabled.
  EXPECT_FALSE(privacy_sandbox_settings()
                   ->IsAttributionReportingTransitionalDebuggingAllowed(
                       url::Origin::Create(GURL("https://test.com")),
                       url::Origin::Create(GURL("https://embedded.com")),
                       ara_transitional_debug_reporting_can_bypass));
  EXPECT_EQ(ara_transitional_debug_reporting_can_bypass, enabled);

  // Disallowed due to user's exception, therefore cannot bypass.
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));
  host_content_settings_map()->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("https://embedded.com"),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
      ContentSetting::CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(privacy_sandbox_settings()
                   ->IsAttributionReportingTransitionalDebuggingAllowed(
                       url::Origin::Create(GURL("https://test.com")),
                       url::Origin::Create(GURL("https://embedded.com")),
                       ara_transitional_debug_reporting_can_bypass));
  EXPECT_FALSE(ara_transitional_debug_reporting_can_bypass);
}

struct PrivateAggregationDebugModeTestCase {
  using TupleT = std::tuple<bool, bool, bool, bool, bool, bool>;

  explicit PrivateAggregationDebugModeTestCase(TupleT t)
      : bypass_feature_enabled(std::get<0>(t)),
        cookies_blocked_by_experiment(std::get<1>(t)),
        cookies_blocked_by_user_setting(std::get<2>(t)),
        cookie_controls_mode_ui_pref(std::get<3>(t)),
        site_exception_user_setting_defined(std::get<4>(t)),
        ignore_site_exception_feature_enabled(std::get<5>(t)) {}

  bool bypass_feature_enabled = false;
  bool cookies_blocked_by_experiment = false;
  bool cookies_blocked_by_user_setting = false;
  bool cookie_controls_mode_ui_pref = false;
  bool site_exception_user_setting_defined = false;
  bool ignore_site_exception_feature_enabled = false;
};

class PrivacySandboxSettingsPrivateAggregationDebugModeTest
    : public PrivacySandboxSettingsTest,
      public testing::WithParamInterface<PrivateAggregationDebugModeTestCase> {
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PrivacySandboxSettingsPrivateAggregationDebugModeTest,
    testing::ConvertGenerator<PrivateAggregationDebugModeTestCase::TupleT>(
        testing::Combine(testing::Bool(),
                         testing::Bool(),
                         testing::Bool(),
                         testing::Bool(),
                         testing::Bool(),
                         testing::Bool())),
    // Creates a human-readable name for each test. Per gtest docs, test names
    // must contain only alphanumeric characters.
    [](const testing::TestParamInfo<PrivateAggregationDebugModeTestCase>& info)
        -> std::string {
      return base::StringPrintf(
          "BypassFeature%s"
          "And3pcdExperiment%s"
          "AndExplicitUserSetting%s"
          "AndCookieControlsModePref%s"
          "AndSiteExceptionUserSetting%s"
          "AndIgnoreSiteException%s",
          info.param.bypass_feature_enabled ? "On" : "Off",
          info.param.cookies_blocked_by_experiment ? "On" : "Off",
          info.param.cookies_blocked_by_user_setting ? "Blocks3pc" : "IsNotSet",
          info.param.cookie_controls_mode_ui_pref ? "On" : "Off",
          info.param.site_exception_user_setting_defined ? "Defined"
                                                         : "NotDefined",
          info.param.ignore_site_exception_feature_enabled ? "On" : "Off");
    });

// Test that Private Aggregation Debug Mode can be enabled in some circumstances
// even though third-party cookies are blocked.
TEST_P(PrivacySandboxSettingsPrivateAggregationDebugModeTest,
       IsPrivateAggregationDebugModeAllowed) {
  // Debug Mode should be disabled when third-party cookies are blocked,
  // unless all of the following are true:
  //   1. The bypass feature is enabled.
  //   2. Third-party cookies were blocked due to the 3PCD experiment.
  //   3. Third-party cookies were not blocked due to an explicit user setting.
  //
  // Additionally, if third-party cookies are re-enabled with a top-level site
  // exception, that will allow for debug mode unless the ignore site exception
  // feature is enabled.
  //
  // Note that `test_case.cookie_controls_mode_pref` does not affect the value
  // of `expect_debug_mode`.
  const PrivateAggregationDebugModeTestCase& test_case = GetParam();
  const bool expect_debug_mode =
      (test_case.bypass_feature_enabled &&
       test_case.cookies_blocked_by_experiment &&
       !test_case.cookies_blocked_by_user_setting) ||
      (test_case.site_exception_user_setting_defined &&
       !test_case.ignore_site_exception_feature_enabled);

  base::test::ScopedFeatureList feature_list;
  std::vector<base::test::FeatureRef> enabled_features = {
      content_settings::features::kTrackingProtection3pcd};
  std::vector<base::test::FeatureRef> disabled_features = {};
  if (test_case.bypass_feature_enabled) {
    enabled_features.emplace_back(
        kPrivateAggregationDebugReportingCookieDeprecationTesting);
  } else {
    disabled_features.emplace_back(
        kPrivateAggregationDebugReportingCookieDeprecationTesting);
  }
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

  ON_CALL(*mock_delegate(),
          AreThirdPartyCookiesBlockedByCookieDeprecationExperiment())
      .WillByDefault(testing::Return(test_case.cookies_blocked_by_experiment));

  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           test_case.cookie_controls_mode_ui_pref)));
  if (test_case.cookies_blocked_by_user_setting) {
    host_content_settings_map()->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString("https://embedded.com"),
        ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
        ContentSetting::CONTENT_SETTING_BLOCK);
  }
  if (test_case.site_exception_user_setting_defined) {
    host_content_settings_map()->SetContentSettingCustomScope(
        ContentSettingsPattern::FromString("https://embedded.com"),
        ContentSettingsPattern::FromString("https://test.com"),
        ContentSettingsType::COOKIES, ContentSetting::CONTENT_SETTING_ALLOW);
  }

  const bool is_debug_mode_allowed =
      privacy_sandbox_settings()->IsPrivateAggregationDebugModeAllowed(
          url::Origin::Create(GURL("https://test.com")),
          url::Origin::Create(GURL("https://embedded.com")));

  EXPECT_EQ(is_debug_mode_allowed, expect_debug_mode);
}

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

TEST_F(PrivacySandboxSettingsMockDelegateTest, IsSubjectToM1NoticeRestricted) {
  // The settings should return the decision made by the delegate.
  EXPECT_CALL(*mock_delegate(), IsSubjectToM1NoticeRestricted())
      .Times(1)
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(privacy_sandbox_settings()->IsSubjectToM1NoticeRestricted());

  EXPECT_CALL(*mock_delegate(), IsSubjectToM1NoticeRestricted())
      .Times(1)
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(privacy_sandbox_settings()->IsSubjectToM1NoticeRestricted());
}

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
        mock_delegate(), mock_browsing_topics_service(),
        privacy_sandbox_settings(), nullptr, user_provider_raw,
        managed_provider_raw, TestCase(test_state, test_input, test_output));
  }

 protected:
  // Pseudo-constants for the convenience of tests that need to check values of
  // type bool*. We can't actually make these const, as then dereferencing them
  // would give the wrong type (i.e. const bool*). Since we are using
  // absl::variant for the test outputs, the types must exactly match.
  bool kTrue_ = true;
  bool kFalse_ = false;

  bool actual_out_shared_storage_block_is_site_setting_specific_ = false;
  bool actual_out_select_url_block_is_site_setting_specific_ = false;
  bool actual_out_private_aggregation_block_is_site_setting_specific_ = false;

 private:
  bool test_case_run_ = false;
};

TEST_F(PrivacySandboxSettingsM1Test, ApiPreferenceEnabled) {
  // Confirm that M1 kAPI respect M1 targeted preferences when enabled.
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue},
                 true}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))},

          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_},
          {kOutSharedStorageSelectURLBlockIsSiteSettingSpecific,
           &actual_out_select_url_block_is_site_setting_specific_},
          {kOutPrivateAggregationBlockIsSiteSettingSpecific,
           &actual_out_private_aggregation_block_is_site_setting_specific_}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowed, kIsTopicsAllowedForContext,
               kIsFledgeJoinAllowed, kIsFledgeLeaveAllowed,
               kIsFledgeUpdateAllowed, kIsFledgeSellAllowed,
               kIsFledgeBuyAllowed, kIsAttributionReportingEverAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed,
               kIsLocalUnpartitionedDataAccessAllowed},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric, kIsTopicsAllowedForContextMetric,
               kIsFledgeJoinAllowedMetric, kIsFledgeLeaveAllowedMetric,
               kIsFledgeUpdateAllowedMetric, kIsFledgeSellAllowedMetric,
               kIsFledgeBuyAllowedMetric,
               kIsAttributionReportingEverAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{kIsSharedStorageBlockSiteSettingSpecific,
                              kIsSharedStorageSelectURLBlockSiteSettingSpecific,
                              kIsPrivateAggregationBlockSiteSettingSpecific},
           &kFalse_}});
}

TEST_F(PrivacySandboxSettingsM1Test, ApiPreferenceDisabled) {
  // Confirm that M1 kAPI respect M1 targeted preferences when disabled.
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue},
                 false}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))},

          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_},
          {kOutSharedStorageSelectURLBlockIsSiteSettingSpecific,
           &actual_out_select_url_block_is_site_setting_specific_},
          {kOutPrivateAggregationBlockIsSiteSettingSpecific,
           &actual_out_private_aggregation_block_is_site_setting_specific_}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowed, kIsTopicsAllowedForContext,
               kIsFledgeJoinAllowed, kIsFledgeLeaveAllowed,
               kIsFledgeUpdateAllowed, kIsFledgeSellAllowed,
               kIsFledgeBuyAllowed, kIsAttributionReportingEverAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageSelectURLAllowed, kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed},
           false},
          {MultipleOutputKeys{kIsSharedStorageAllowed,
                              kIsLocalUnpartitionedDataAccessAllowed},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric, kIsTopicsAllowedForContextMetric,
               kIsFledgeJoinAllowedMetric, kIsFledgeLeaveAllowedMetric,
               kIsFledgeUpdateAllowedMetric, kIsFledgeSellAllowedMetric,
               kIsFledgeBuyAllowedMetric,
               kIsAttributionReportingEverAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric},
           static_cast<int>(Status::kApisDisabled)},
          {MultipleOutputKeys{kIsSharedStorageAllowedMetric,
                              kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{kIsSharedStorageBlockSiteSettingSpecific,
                              kIsSharedStorageSelectURLBlockSiteSettingSpecific,
                              kIsPrivateAggregationBlockSiteSettingSpecific},
           &kFalse_}});
}

TEST_F(
    PrivacySandboxSettingsM1Test,
    CookieControlsModeEffectsOnlyPrivateAggregationDebugModeAndFencedFrameLocalUnpartitionedDataAccess) {
  // Confirm that Private Aggregation Debug Mode and fenced frame local
  // unpartitioned data are the only M1 kAPIs affected by 3PC blocking.
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue},
                 true},
                {kCookieControlsModeUserPrefValue, kBlockThirdParty}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowed, kIsTopicsAllowedForContext,
               kIsFledgeJoinAllowed, kIsFledgeLeaveAllowed,
               kIsFledgeUpdateAllowed, kIsFledgeSellAllowed,
               kIsFledgeBuyAllowed, kIsAttributionReportingEverAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed},
           true},
          {MultipleOutputKeys{kIsPrivateAggregationDebugModeAllowed,
                              kIsLocalUnpartitionedDataAccessAllowed},
           false},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric, kIsTopicsAllowedForContextMetric,
               kIsFledgeJoinAllowedMetric, kIsFledgeLeaveAllowedMetric,
               kIsFledgeUpdateAllowedMetric, kIsFledgeSellAllowedMetric,
               kIsFledgeBuyAllowedMetric,
               kIsAttributionReportingEverAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric},
           static_cast<int>(Status::kAllowed)},
          {kIsLocalUnpartitionedDataAccessAllowedMetric,
           static_cast<int>(Status::kApisDisabled)}});
}

TEST_F(PrivacySandboxSettingsM1Test, SiteDataDefaultBlockExceptionApplies) {
  // Confirm that blocking site data for a site disables M1 kAPIs, with the
  // exception of the generic IsTopicsAllowed(). Topics should still be able
  // to calculate, as sites with ALLOW exceptions may still access site data.
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue},
                 true},
                {kSiteDataUserDefault, CONTENT_SETTING_BLOCK}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))},

          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_},
          {kOutSharedStorageSelectURLBlockIsSiteSettingSpecific,
           &actual_out_select_url_block_is_site_setting_specific_},
          {kOutPrivateAggregationBlockIsSiteSettingSpecific,
           &actual_out_private_aggregation_block_is_site_setting_specific_}},
      TestOutput{
          {MultipleOutputKeys{kIsTopicsAllowed,
                              kIsAttributionReportingEverAllowed},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContext, kIsFledgeJoinAllowed,
               kIsFledgeLeaveAllowed, kIsFledgeUpdateAllowed,
               kIsFledgeSellAllowed, kIsFledgeBuyAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed,
               kIsCookieDeprecationLabelAllowedForContext,
               kIsLocalUnpartitionedDataAccessAllowed},
           false},
          {MultipleOutputKeys{kIsTopicsAllowedMetric,
                              kIsAttributionReportingEverAllowedMetric},
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContextMetric, kIsFledgeJoinAllowedMetric,
               kIsFledgeLeaveAllowedMetric, kIsFledgeUpdateAllowedMetric,
               kIsFledgeSellAllowedMetric, kIsFledgeBuyAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kSiteDataAccessBlocked)},
          {MultipleOutputKeys{kIsSharedStorageBlockSiteSettingSpecific,
                              kIsSharedStorageSelectURLBlockSiteSettingSpecific,
                              kIsPrivateAggregationBlockSiteSettingSpecific},
           &kTrue_}});
}

TEST_F(PrivacySandboxSettingsM1Test, SiteDataBlockExceptionApplies) {
  // Confirm that blocking site data for a site disabled M1 kAPIs.
  RunTestCase(
      TestState{
          {MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                             kM1FledgeEnabledUserPrefValue,
                             kM1AdMeasurementEnabledUserPrefValue},
           true},
          {kSiteDataUserDefault, CONTENT_SETTING_ALLOW},
          {kSiteDataUserExceptions,
           SiteDataExceptions{{"[*.]embedded.com", CONTENT_SETTING_BLOCK}}}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{kIsTopicsAllowed,
                              kIsAttributionReportingEverAllowed},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContext, kIsFledgeJoinAllowed,
               kIsFledgeLeaveAllowed, kIsFledgeUpdateAllowed,
               kIsFledgeSellAllowed, kIsFledgeBuyAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed,
               kIsCookieDeprecationLabelAllowedForContext,
               kIsLocalUnpartitionedDataAccessAllowed},
           false},
          {MultipleOutputKeys{kIsTopicsAllowedMetric,
                              kIsAttributionReportingEverAllowedMetric},
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContextMetric, kIsFledgeJoinAllowedMetric,
               kIsFledgeLeaveAllowedMetric, kIsFledgeUpdateAllowedMetric,
               kIsFledgeSellAllowedMetric, kIsFledgeBuyAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kSiteDataAccessBlocked)}});
}

TEST_F(PrivacySandboxSettingsM1Test, SiteDataAllowDoesntOverridePref) {
  // Confirm that allowing site data doesn't override preference values, even
  // via exceptions.
  RunTestCase(
      TestState{
          {MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                             kM1FledgeEnabledUserPrefValue,
                             kM1AdMeasurementEnabledUserPrefValue},
           false},
          {kSiteDataUserDefault, CONTENT_SETTING_ALLOW},
          {kSiteDataUserExceptions,
           SiteDataExceptions{{"[*.]embedded.com", CONTENT_SETTING_ALLOW},
                              {"[*.]top-frame.com", CONTENT_SETTING_ALLOW}}}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{kIsSharedStorageAllowed,
                              kIsCookieDeprecationLabelAllowedForContext,
                              kIsLocalUnpartitionedDataAccessAllowed},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowed, kIsTopicsAllowedForContext,
               kIsFledgeJoinAllowed, kIsFledgeLeaveAllowed,
               kIsFledgeUpdateAllowed, kIsFledgeSellAllowed,
               kIsFledgeBuyAllowed, kIsAttributionReportingEverAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageSelectURLAllowed, kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed},
           false},
          {MultipleOutputKeys{kIsSharedStorageAllowedMetric,
                              kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric, kIsTopicsAllowedForContextMetric,
               kIsFledgeJoinAllowedMetric, kIsFledgeLeaveAllowedMetric,
               kIsFledgeUpdateAllowedMetric, kIsFledgeSellAllowedMetric,
               kIsFledgeBuyAllowedMetric,
               kIsAttributionReportingEverAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric},
           static_cast<int>(Status::kApisDisabled)}});
}

TEST_F(PrivacySandboxSettingsM1Test, SiteDataAllowExceptions) {
  // Confirm that site data exceptions override the default site data setting.
  RunTestCase(
      TestState{
          {MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                             kM1FledgeEnabledUserPrefValue,
                             kM1AdMeasurementEnabledUserPrefValue},
           true},
          {kSiteDataUserDefault, CONTENT_SETTING_BLOCK},
          {kSiteDataUserExceptions,
           SiteDataExceptions{{"[*.]embedded.com", CONTENT_SETTING_ALLOW}}}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowed, kIsTopicsAllowedForContext,
               kIsFledgeJoinAllowed, kIsFledgeLeaveAllowed,
               kIsFledgeUpdateAllowed, kIsFledgeSellAllowed,
               kIsFledgeBuyAllowed, kIsAttributionReportingEverAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed,
               kIsCookieDeprecationLabelAllowedForContext,
               kIsLocalUnpartitionedDataAccessAllowed},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric, kIsTopicsAllowedForContextMetric,
               kIsFledgeJoinAllowedMetric, kIsFledgeLeaveAllowedMetric,
               kIsFledgeUpdateAllowedMetric, kIsFledgeSellAllowedMetric,
               kIsFledgeBuyAllowedMetric, kIsAttributionReportingAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kAllowed)}});
}

TEST_F(PrivacySandboxSettingsM1Test, UnrelatedSiteDataBlock) {
  // Confirm that unrelated site data block exceptions don't affect kAPIs.
  RunTestCase(
      TestState{
          {MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                             kM1FledgeEnabledUserPrefValue,
                             kM1AdMeasurementEnabledUserPrefValue},
           true},
          {kSiteDataUserDefault, CONTENT_SETTING_ALLOW},
          {kSiteDataUserExceptions,
           SiteDataExceptions{{"[*.]unrelated.com", CONTENT_SETTING_BLOCK}}}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowed, kIsTopicsAllowedForContext,
               kIsFledgeJoinAllowed, kIsFledgeLeaveAllowed,
               kIsFledgeUpdateAllowed, kIsFledgeSellAllowed,
               kIsFledgeBuyAllowed, kIsAttributionReportingEverAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed,
               kIsCookieDeprecationLabelAllowedForContext,
               kIsLocalUnpartitionedDataAccessAllowed},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric, kIsTopicsAllowedForContextMetric,
               kIsFledgeJoinAllowedMetric, kIsFledgeLeaveAllowedMetric,
               kIsFledgeUpdateAllowedMetric, kIsFledgeSellAllowedMetric,
               kIsFledgeBuyAllowedMetric,
               kIsAttributionReportingEverAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kAllowed)}});
}

TEST_F(PrivacySandboxSettingsM1Test, UnrelatedSiteDataAllow) {
  // Confirm that unrelated site data allow exceptions don't affect kAPIs.
  RunTestCase(
      TestState{
          {MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                             kM1FledgeEnabledUserPrefValue,
                             kM1AdMeasurementEnabledUserPrefValue},
           true},
          {kSiteDataUserDefault, CONTENT_SETTING_BLOCK},
          {kSiteDataUserExceptions,
           SiteDataExceptions{{"[*.]unrelated.com", CONTENT_SETTING_ALLOW}}}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{kIsTopicsAllowed,
                              kIsAttributionReportingEverAllowed},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContext, kIsFledgeJoinAllowed,
               kIsFledgeLeaveAllowed, kIsFledgeUpdateAllowed,
               kIsFledgeSellAllowed, kIsFledgeBuyAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed,
               kIsCookieDeprecationLabelAllowedForContext,
               kIsLocalUnpartitionedDataAccessAllowed},
           false},
          {MultipleOutputKeys{kIsTopicsAllowedMetric,
                              kIsAttributionReportingEverAllowedMetric},
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContextMetric, kIsFledgeJoinAllowedMetric,
               kIsFledgeLeaveAllowedMetric, kIsFledgeUpdateAllowedMetric,
               kIsFledgeSellAllowedMetric, kIsFledgeBuyAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kSiteDataAccessBlocked)}});
}

TEST_F(PrivacySandboxSettingsM1Test, ApisAreOffInIncognito) {
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue,
                                   kIsIncognito},
                 true}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))},

          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_},
          {kOutSharedStorageSelectURLBlockIsSiteSettingSpecific,
           &actual_out_select_url_block_is_site_setting_specific_},
          {kOutPrivateAggregationBlockIsSiteSettingSpecific,
           &actual_out_private_aggregation_block_is_site_setting_specific_}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowed, kIsTopicsAllowedForContext,
               kIsFledgeJoinAllowed, kIsFledgeLeaveAllowed,
               kIsFledgeUpdateAllowed, kIsFledgeSellAllowed,
               kIsFledgeBuyAllowed, kIsAttributionReportingEverAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed,
               kIsLocalUnpartitionedDataAccessAllowed},
           false},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric, kIsTopicsAllowedForContextMetric,
               kIsFledgeJoinAllowedMetric, kIsFledgeLeaveAllowedMetric,
               kIsFledgeUpdateAllowedMetric, kIsFledgeSellAllowedMetric,
               kIsFledgeBuyAllowedMetric,
               kIsAttributionReportingEverAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kIncognitoProfile)},
          {MultipleOutputKeys{kIsSharedStorageBlockSiteSettingSpecific,
                              kIsSharedStorageSelectURLBlockSiteSettingSpecific,
                              kIsPrivateAggregationBlockSiteSettingSpecific},
           &kFalse_}});
}

TEST_F(PrivacySandboxSettingsM1Test, ApisAreOffForRestrictedAccounts) {
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue,
                                   kIsRestrictedAccount},
                 true}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))},

          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_},
          {kOutSharedStorageSelectURLBlockIsSiteSettingSpecific,
           &actual_out_select_url_block_is_site_setting_specific_},
          {kOutPrivateAggregationBlockIsSiteSettingSpecific,
           &actual_out_private_aggregation_block_is_site_setting_specific_}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowed, kIsTopicsAllowedForContext,
               kIsFledgeJoinAllowed, kIsFledgeLeaveAllowed,
               kIsFledgeUpdateAllowed, kIsFledgeSellAllowed,
               kIsFledgeBuyAllowed, kIsAttributionReportingEverAllowed,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed,
               kIsLocalUnpartitionedDataAccessAllowed},
           false},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric, kIsTopicsAllowedForContextMetric,
               kIsFledgeJoinAllowedMetric, kIsFledgeLeaveAllowedMetric,
               kIsFledgeUpdateAllowedMetric, kIsFledgeSellAllowedMetric,
               kIsFledgeBuyAllowedMetric,
               kIsAttributionReportingEverAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kRestricted)},
          {MultipleOutputKeys{kIsSharedStorageBlockSiteSettingSpecific,
                              kIsSharedStorageSelectURLBlockSiteSettingSpecific,
                              kIsPrivateAggregationBlockSiteSettingSpecific},
           &kFalse_}});
}

TEST_F(PrivacySandboxSettingsM1Test,
       CheckFledgeDependentApi_FledgeOn_OtherApiOn) {
  RunTestCase(TestState{{kM1FledgeEnabledUserPrefValue, true}},
              TestInput{{kTopFrameOrigin,
                         url::Origin::Create(GURL("https://top-frame.com"))},
                        {kAccessingOrigin,
                         url::Origin::Create(GURL("https://embedded.com"))}},
              TestOutput{{kIsSharedStorageSelectURLAllowed, true}});
}

TEST_F(PrivacySandboxSettingsM1Test,
       CheckFledgeDependentApi_FledgeOff_OtherApiOff) {
  RunTestCase(TestState{{kM1FledgeEnabledUserPrefValue, false}},
              TestInput{{kTopFrameOrigin,
                         url::Origin::Create(GURL("https://top-frame.com"))},
                        {kAccessingOrigin,
                         url::Origin::Create(GURL("https://embedded.com"))}},
              TestOutput{{kIsSharedStorageSelectURLAllowed, false}});
}

TEST_F(PrivacySandboxSettingsM1Test,
       CheckAdMeasurementDependentApi_AdMeasurementOn_OtherApiOn) {
  RunTestCase(TestState{{kM1AdMeasurementEnabledUserPrefValue, true}},
              TestInput{{kTopFrameOrigin,
                         url::Origin::Create(GURL("https://top-frame.com"))},
                        {kAdMeasurementReportingOrigin,
                         url::Origin::Create(GURL("https://embedded.com"))}},
              TestOutput{{kIsPrivateAggregationAllowed, true}});
}

TEST_F(PrivacySandboxSettingsM1Test,
       CheckAdMeasurementDependentApi_AdMeasurementOff_OtherApiOff) {
  RunTestCase(TestState{{kM1AdMeasurementEnabledUserPrefValue, false}},
              TestInput{{kTopFrameOrigin,
                         url::Origin::Create(GURL("https://top-frame.com"))},
                        {kAdMeasurementReportingOrigin,
                         url::Origin::Create(GURL("https://embedded.com"))}},
              TestOutput{{kIsPrivateAggregationAllowed, false}});
}

TEST_F(PrivacySandboxSettingsM1Test, NoAppropriateTopicsConsent) {
  // Confirm that when appropriate Topics consent is missing, Topics is disabled
  // while other APIs are unaffected.
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue},
                 true},
                {kHasAppropriateTopicsConsent, false}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{
               kIsFledgeJoinAllowed, kIsFledgeLeaveAllowed,
               kIsFledgeUpdateAllowed, kIsFledgeSellAllowed,
               kIsFledgeBuyAllowed, kIsAttributionReportingAllowed,
               kIsAttributionReportingEverAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsSharedStorageSelectURLAllowed,
               kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed,
               kIsLocalUnpartitionedDataAccessAllowed},
           true},
          {MultipleOutputKeys{kIsTopicsAllowed, kIsTopicsAllowedForContext},
           false},
          {MultipleOutputKeys{
               kIsFledgeJoinAllowedMetric, kIsFledgeLeaveAllowedMetric,
               kIsFledgeUpdateAllowedMetric, kIsFledgeSellAllowedMetric,
               kIsFledgeBuyAllowedMetric,
               kIsAttributionReportingEverAllowedMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsPrivateAggregationAllowedMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric,
               kIsTopicsAllowedForContextMetric,
           },
           static_cast<int>(Status::kMismatchedConsent)}});
}

TEST_F(PrivacySandboxSettingsM1Test, TopicsConsentStatus) {
  // Confirm that if Topics is already disabled, and there is no appropriate
  // consent, the recorded status reflects that Topics is already disabled.
  RunTestCase(
      TestState{{kM1TopicsEnabledUserPrefValue, false},
                {kHasAppropriateTopicsConsent, false}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
      },
      TestOutput{
          {MultipleOutputKeys{kIsTopicsAllowed, kIsTopicsAllowedForContext},
           false},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric,
               kIsTopicsAllowedForContextMetric,
           },
           static_cast<int>(Status::kApisDisabled)}});
}

class PrivacySandboxSettingsM1RestrictedNotice
    : public PrivacySandboxSettingsM1Test {
  void InitializeFeaturesBeforeStart() override {
    // Do not set default handlers so each call must be mocked.
  }
};

TEST_F(PrivacySandboxSettingsM1RestrictedNotice,
       AllApisAreOffExceptMeasurementForRestrictedAccounts) {
  ON_CALL(*mock_delegate(), IsRestrictedNoticeEnabled())
      .WillByDefault(testing::Return(true));
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue,
                                   kIsRestrictedAccount},
                 true}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
          {MultipleInputKeys{kFledgeAuctionPartyOrigin,
                             kAdMeasurementReportingOrigin, kAccessingOrigin},
           url::Origin::Create(GURL("https://embedded.com"))},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL("https://source-origin.com"))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL("https://dest-origin.com"))}},
      TestOutput{
          {MultipleOutputKeys{kIsTopicsAllowed, kIsTopicsAllowedForContext,
                              kIsFledgeJoinAllowed, kIsFledgeLeaveAllowed,
                              kIsFledgeUpdateAllowed, kIsFledgeSellAllowed,
                              kIsFledgeBuyAllowed, kIsSharedStorageAllowed,
                              kIsSharedStorageSelectURLAllowed,
                              kIsLocalUnpartitionedDataAccessAllowed},
           false},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric, kIsTopicsAllowedForContextMetric,
               kIsFledgeJoinAllowedMetric, kIsFledgeLeaveAllowedMetric,
               kIsFledgeUpdateAllowedMetric, kIsFledgeSellAllowedMetric,
               kIsFledgeBuyAllowedMetric, kIsSharedStorageAllowedMetric,
               kIsSharedStorageSelectURLAllowedMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kRestricted)},

          {MultipleOutputKeys{kIsAttributionReportingAllowed,
                              kIsAttributionReportingEverAllowed,
                              kMaySendAttributionReport,
                              kIsPrivateAggregationAllowed,
                              kIsPrivateAggregationDebugModeAllowed},
           true},
          {MultipleOutputKeys{kIsAttributionReportingEverAllowedMetric,
                              kMaySendAttributionReportMetric,
                              kIsPrivateAggregationAllowedMetric},
           static_cast<int>(Status::kAllowed)}});
}

class PrivacySandboxAttestationsTest : public base::test::WithFeatureOverride,
                                       public PrivacySandboxSettingsM1Test {
 public:
  PrivacySandboxAttestationsTest()
      : base::test::WithFeatureOverride(
            kDefaultAllowPrivacySandboxAttestations) {
    // This test suite tests Privacy Sandbox Attestations related behaviors,
    // turn off the setting that makes all APIs considered attested.
    privacy_sandbox::PrivacySandboxAttestations::GetInstance()
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
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAccessingOrigin, url::Origin::Create(enrollee_url)},

          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_},
          {kOutPrivateAggregationBlockIsSiteSettingSpecific,
           &actual_out_private_aggregation_block_is_site_setting_specific_}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowedForContext, kIsAttributionReportingAllowed,
               kMaySendAttributionReport, kIsFledgeJoinAllowed,
               kIsFledgeLeaveAllowed, kIsFledgeUpdateAllowed,
               kIsFledgeSellAllowed, kIsFledgeBuyAllowed,
               kIsEventReportingDestinationAttestedForFledge,
               kIsEventReportingDestinationAttestedForSharedStorage,
               kIsSharedStorageAllowed, kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed,
               kIsLocalUnpartitionedDataAccessAllowed},
           IsAttestationsDefaultAllowed()},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContextMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsFledgeJoinAllowedMetric,
               kIsFledgeLeaveAllowedMetric, kIsFledgeUpdateAllowedMetric,
               kIsFledgeSellAllowedMetric, kIsFledgeBuyAllowedMetric,
               kIsSharedStorageAllowedMetric,
               kIsPrivateAggregationAllowedMetric,
               kIsEventReportingDestinationAttestedForSharedStorageMetric,
               kIsEventReportingDestinationAttestedForFledgeMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
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
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAccessingOrigin, url::Origin::Create(enrollee_url)},

          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_},
          {kOutPrivateAggregationBlockIsSiteSettingSpecific,
           &actual_out_private_aggregation_block_is_site_setting_specific_}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowedForContext, kIsAttributionReportingAllowed,
               kMaySendAttributionReport, kIsFledgeJoinAllowed,
               kIsFledgeLeaveAllowed, kIsFledgeUpdateAllowed,
               kIsFledgeSellAllowed, kIsFledgeBuyAllowed,
               kIsEventReportingDestinationAttestedForFledge,
               kIsEventReportingDestinationAttestedForSharedStorage,
               kIsSharedStorageAllowed, kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed,
               kIsLocalUnpartitionedDataAccessAllowed},
           false},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContextMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsFledgeJoinAllowedMetric,
               kIsFledgeLeaveAllowedMetric, kIsFledgeUpdateAllowedMetric,
               kIsFledgeSellAllowedMetric, kIsFledgeBuyAllowedMetric,
               kIsSharedStorageAllowedMetric,
               kIsPrivateAggregationAllowedMetric,
               kIsEventReportingDestinationAttestedForSharedStorageMetric,
               kIsEventReportingDestinationAttestedForFledgeMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
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
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAccessingOrigin, url::Origin::Create(enrollee_url)}},
      TestOutput{
          {MultipleOutputKeys{
               kIsTopicsAllowedForContext, kIsAttributionReportingAllowed,
               kMaySendAttributionReport, kIsFledgeJoinAllowed,
               kIsFledgeLeaveAllowed, kIsFledgeUpdateAllowed,
               kIsFledgeSellAllowed, kIsFledgeBuyAllowed,
               kIsEventReportingDestinationAttestedForFledge,
               kIsEventReportingDestinationAttestedForSharedStorage,
               kIsSharedStorageAllowed, kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed,
               kIsLocalUnpartitionedDataAccessAllowed},
           false},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContextMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsFledgeJoinAllowedMetric,
               kIsFledgeLeaveAllowedMetric, kIsFledgeUpdateAllowedMetric,
               kIsFledgeSellAllowedMetric, kIsFledgeBuyAllowedMetric,
               kIsSharedStorageAllowedMetric,
               kIsPrivateAggregationAllowedMetric,
               kIsEventReportingDestinationAttestedForSharedStorageMetric,
               kIsEventReportingDestinationAttestedForFledgeMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kAttestationFailed)}});
}

TEST_P(PrivacySandboxAttestationsTest, TopicsAttestation) {
  GURL top_frame_url("https://top-frame.com");
  GURL enrollee_url("https://embedded.com");
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue},
                 true},
                {kAttestationsMap,
                 PrivacySandboxAttestationsMap{
                     {net::SchemefulSite(enrollee_url),
                      {PrivacySandboxAttestationsGatedAPI::kTopics}}}}},
      TestInput{
          {kTopicsURL, enrollee_url},
          {kTopFrameOrigin, url::Origin::Create(top_frame_url)},
          {kAdMeasurementReportingOrigin, url::Origin::Create(enrollee_url)},
          {kFledgeAuctionPartyOrigin, url::Origin::Create(enrollee_url)},
          {kEventReportingDestinationOrigin, url::Origin::Create(enrollee_url)},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAccessingOrigin, url::Origin::Create(enrollee_url)}},
      TestOutput{
          {kIsTopicsAllowedForContext, true},
          {MultipleOutputKeys{
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsFledgeJoinAllowed, kIsFledgeLeaveAllowed,
               kIsFledgeUpdateAllowed, kIsFledgeSellAllowed,
               kIsFledgeBuyAllowed, kIsSharedStorageAllowed,
               kIsEventReportingDestinationAttestedForFledge,
               kIsEventReportingDestinationAttestedForSharedStorage,
               kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed,
               kIsLocalUnpartitionedDataAccessAllowed},
           false},
          {kIsTopicsAllowedForContextMetric,
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsFledgeJoinAllowedMetric,
               kIsFledgeLeaveAllowedMetric, kIsFledgeUpdateAllowedMetric,
               kIsFledgeSellAllowedMetric, kIsFledgeBuyAllowedMetric,
               kIsSharedStorageAllowedMetric,
               kIsPrivateAggregationAllowedMetric,
               kIsEventReportingDestinationAttestedForSharedStorageMetric,
               kIsEventReportingDestinationAttestedForFledgeMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kAttestationFailed)}});
}

TEST_P(PrivacySandboxAttestationsTest, PrivateAggregationAttestation) {
  GURL top_frame_url("https://top-frame.com");
  GURL enrollee_url("https://embedded.com");
  RunTestCase(
      TestState{
          {MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                             kM1FledgeEnabledUserPrefValue,
                             kM1AdMeasurementEnabledUserPrefValue},
           true},
          {kAttestationsMap,
           PrivacySandboxAttestationsMap{
               {net::SchemefulSite(enrollee_url),
                {PrivacySandboxAttestationsGatedAPI::kPrivateAggregation}}}}},
      TestInput{
          {kTopicsURL, enrollee_url},
          {kTopFrameOrigin, url::Origin::Create(top_frame_url)},
          {kAdMeasurementReportingOrigin, url::Origin::Create(enrollee_url)},
          {kFledgeAuctionPartyOrigin, url::Origin::Create(enrollee_url)},
          {kEventReportingDestinationOrigin, url::Origin::Create(enrollee_url)},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAccessingOrigin, url::Origin::Create(enrollee_url)},

          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_},
          {kOutPrivateAggregationBlockIsSiteSettingSpecific,
           &actual_out_private_aggregation_block_is_site_setting_specific_}},
      TestOutput{
          {MultipleOutputKeys{kIsPrivateAggregationAllowed,
                              kIsPrivateAggregationDebugModeAllowed},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContext, kIsAttributionReportingAllowed,
               kMaySendAttributionReport, kIsFledgeJoinAllowed,
               kIsFledgeLeaveAllowed, kIsFledgeUpdateAllowed,
               kIsFledgeSellAllowed, kIsFledgeBuyAllowed,
               kIsEventReportingDestinationAttestedForFledge,
               kIsEventReportingDestinationAttestedForSharedStorage,
               kIsSharedStorageAllowed, kIsLocalUnpartitionedDataAccessAllowed},
           false},
          {kIsPrivateAggregationAllowedMetric,
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContextMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsFledgeJoinAllowedMetric,
               kIsFledgeLeaveAllowedMetric, kIsFledgeUpdateAllowedMetric,
               kIsFledgeSellAllowedMetric, kIsFledgeBuyAllowedMetric,
               kIsSharedStorageAllowedMetric,
               kIsEventReportingDestinationAttestedForSharedStorageMetric,
               kIsEventReportingDestinationAttestedForFledgeMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kAttestationFailed)},
          {MultipleOutputKeys{kIsSharedStorageBlockSiteSettingSpecific,
                              kIsPrivateAggregationBlockSiteSettingSpecific},
           &kFalse_}});
}

TEST_P(PrivacySandboxAttestationsTest, SharedStorageAttestation) {
  GURL top_frame_url("https://top-frame.com");
  GURL enrollee_url("https://embedded.com");
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue},
                 true},
                {kAttestationsMap,
                 PrivacySandboxAttestationsMap{
                     {net::SchemefulSite(enrollee_url),
                      {PrivacySandboxAttestationsGatedAPI::kSharedStorage}}}}},
      TestInput{
          {kTopicsURL, enrollee_url},
          {kTopFrameOrigin, url::Origin::Create(top_frame_url)},
          {kAdMeasurementReportingOrigin, url::Origin::Create(enrollee_url)},
          {kFledgeAuctionPartyOrigin, url::Origin::Create(enrollee_url)},
          {kEventReportingDestinationOrigin, url::Origin::Create(enrollee_url)},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAccessingOrigin, url::Origin::Create(enrollee_url)},

          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_},
          {kOutPrivateAggregationBlockIsSiteSettingSpecific,
           &actual_out_private_aggregation_block_is_site_setting_specific_}},
      TestOutput{
          {MultipleOutputKeys{
               kIsSharedStorageAllowed,
               kIsEventReportingDestinationAttestedForSharedStorage},
           true},
          {MultipleOutputKeys{kIsTopicsAllowedForContext,
                              kIsAttributionReportingAllowed,
                              kMaySendAttributionReport, kIsFledgeJoinAllowed,
                              kIsFledgeLeaveAllowed, kIsFledgeUpdateAllowed,
                              kIsFledgeSellAllowed, kIsFledgeBuyAllowed,
                              kIsEventReportingDestinationAttestedForFledge,
                              kIsPrivateAggregationAllowed,
                              kIsPrivateAggregationDebugModeAllowed},
           false},
          {MultipleOutputKeys{
               kIsSharedStorageAllowedMetric,
               kIsEventReportingDestinationAttestedForSharedStorageMetric},
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContextMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsFledgeJoinAllowedMetric,
               kIsFledgeLeaveAllowedMetric, kIsFledgeUpdateAllowedMetric,
               kIsFledgeSellAllowedMetric, kIsFledgeBuyAllowedMetric,
               kIsPrivateAggregationAllowedMetric,
               kIsEventReportingDestinationAttestedForFledgeMetric},
           static_cast<int>(Status::kAttestationFailed)},
          {MultipleOutputKeys{kIsSharedStorageBlockSiteSettingSpecific,
                              kIsPrivateAggregationBlockSiteSettingSpecific},
           &kFalse_}});
}

TEST_P(PrivacySandboxAttestationsTest,
       LocalUnpartitionedDataAccessAttestation) {
  GURL top_frame_url("https://top-frame.com");
  GURL enrollee_url("https://embedded.com");
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue},
                 true},
                {kAttestationsMap,
                 PrivacySandboxAttestationsMap{
                     {net::SchemefulSite(enrollee_url),
                      {PrivacySandboxAttestationsGatedAPI::
                           kLocalUnpartitionedDataAccess}}}}},
      TestInput{
          {kTopicsURL, enrollee_url},
          {kTopFrameOrigin, url::Origin::Create(top_frame_url)},
          {kAdMeasurementReportingOrigin, url::Origin::Create(enrollee_url)},
          {kFledgeAuctionPartyOrigin, url::Origin::Create(enrollee_url)},
          {kEventReportingDestinationOrigin, url::Origin::Create(enrollee_url)},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAccessingOrigin, url::Origin::Create(enrollee_url)},
          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_},
          {kOutPrivateAggregationBlockIsSiteSettingSpecific,
           &actual_out_private_aggregation_block_is_site_setting_specific_}},
      TestOutput{
          {kIsLocalUnpartitionedDataAccessAllowed, true},
          {MultipleOutputKeys{kIsTopicsAllowedForContext,
                              kIsAttributionReportingAllowed,
                              kMaySendAttributionReport,
                              kIsSharedStorageAllowed, kIsFledgeJoinAllowed,
                              kIsFledgeLeaveAllowed, kIsFledgeUpdateAllowed,
                              kIsFledgeSellAllowed, kIsFledgeBuyAllowed,
                              kIsEventReportingDestinationAttestedForFledge,
                              kIsPrivateAggregationAllowed,
                              kIsPrivateAggregationDebugModeAllowed},
           false},
          {kIsLocalUnpartitionedDataAccessAllowedMetric,
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContextMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsFledgeJoinAllowedMetric, kIsFledgeLeaveAllowedMetric,
               kIsFledgeUpdateAllowedMetric, kIsFledgeSellAllowedMetric,
               kIsFledgeBuyAllowedMetric, kIsPrivateAggregationAllowedMetric,
               kIsEventReportingDestinationAttestedForFledgeMetric},
           static_cast<int>(Status::kAttestationFailed)},
          {MultipleOutputKeys{kIsSharedStorageBlockSiteSettingSpecific,
                              kIsPrivateAggregationBlockSiteSettingSpecific},
           &kFalse_}});
}

TEST_P(PrivacySandboxAttestationsTest,
       LocalUnpartitionedDataAccessEnabledWhen3pcsLimited) {
  GURL top_frame_url("https://top-frame.com");
  GURL enrollee_url("https://embedded.com");
  RunTestCase(
      TestState{{MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                                   kM1FledgeEnabledUserPrefValue,
                                   kM1AdMeasurementEnabledUserPrefValue},
                 true},
                {kCookieControlsModeUserPrefValue, kLimitedThirdParty},
                {kAttestationsMap,
                 PrivacySandboxAttestationsMap{
                     {net::SchemefulSite(enrollee_url),
                      {PrivacySandboxAttestationsGatedAPI::
                           kLocalUnpartitionedDataAccess}}}}},
      TestInput{
          {kTopicsURL, enrollee_url},
          {kTopFrameOrigin, url::Origin::Create(top_frame_url)},
          {kAdMeasurementReportingOrigin, url::Origin::Create(enrollee_url)},
          {kFledgeAuctionPartyOrigin, url::Origin::Create(enrollee_url)},
          {kEventReportingDestinationOrigin, url::Origin::Create(enrollee_url)},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAccessingOrigin, url::Origin::Create(enrollee_url)},
          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_},
          {kOutPrivateAggregationBlockIsSiteSettingSpecific,
           &actual_out_private_aggregation_block_is_site_setting_specific_}},
      TestOutput{
          {kIsLocalUnpartitionedDataAccessAllowed, true},
          {MultipleOutputKeys{kIsTopicsAllowedForContext,
                              kIsAttributionReportingAllowed,
                              kMaySendAttributionReport,
                              kIsSharedStorageAllowed, kIsFledgeJoinAllowed,
                              kIsFledgeLeaveAllowed, kIsFledgeUpdateAllowed,
                              kIsFledgeSellAllowed, kIsFledgeBuyAllowed,
                              kIsEventReportingDestinationAttestedForFledge,
                              kIsPrivateAggregationAllowed,
                              kIsPrivateAggregationDebugModeAllowed},
           false},
          {kIsLocalUnpartitionedDataAccessAllowedMetric,
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContextMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsFledgeJoinAllowedMetric, kIsFledgeLeaveAllowedMetric,
               kIsFledgeUpdateAllowedMetric, kIsFledgeSellAllowedMetric,
               kIsFledgeBuyAllowedMetric, kIsPrivateAggregationAllowedMetric,
               kIsEventReportingDestinationAttestedForFledgeMetric},
           static_cast<int>(Status::kAttestationFailed)},
          {MultipleOutputKeys{kIsSharedStorageBlockSiteSettingSpecific,
                              kIsPrivateAggregationBlockSiteSettingSpecific},
           &kFalse_}});
}

TEST_P(PrivacySandboxAttestationsTest, FledgeAttestation) {
  GURL top_frame_url("https://top-frame.com");
  GURL enrollee_url("https://embedded.com");
  RunTestCase(
      TestState{
          {MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                             kM1FledgeEnabledUserPrefValue,
                             kM1AdMeasurementEnabledUserPrefValue},
           true},
          {kAttestationsMap,
           PrivacySandboxAttestationsMap{
               {net::SchemefulSite(enrollee_url),
                {PrivacySandboxAttestationsGatedAPI::kProtectedAudience}}}}},
      TestInput{
          {kTopicsURL, enrollee_url},
          {kTopFrameOrigin, url::Origin::Create(top_frame_url)},
          {kAdMeasurementReportingOrigin, url::Origin::Create(enrollee_url)},
          {kFledgeAuctionPartyOrigin, url::Origin::Create(enrollee_url)},
          {kEventReportingDestinationOrigin, url::Origin::Create(enrollee_url)},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAccessingOrigin, url::Origin::Create(enrollee_url)}},
      TestOutput{
          {MultipleOutputKeys{kIsFledgeJoinAllowed, kIsFledgeLeaveAllowed,
                              kIsFledgeUpdateAllowed, kIsFledgeSellAllowed,
                              kIsFledgeBuyAllowed,
                              kIsEventReportingDestinationAttestedForFledge},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContext, kIsAttributionReportingAllowed,
               kMaySendAttributionReport, kIsSharedStorageAllowed,
               kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed,
               kIsEventReportingDestinationAttestedForSharedStorage,
               kIsLocalUnpartitionedDataAccessAllowed},
           false},
          {MultipleOutputKeys{
               kIsFledgeJoinAllowedMetric, kIsFledgeLeaveAllowedMetric,
               kIsFledgeUpdateAllowedMetric, kIsFledgeSellAllowedMetric,
               kIsFledgeBuyAllowedMetric,
               kIsEventReportingDestinationAttestedForFledgeMetric},
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContextMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsPrivateAggregationAllowedMetric,
               kIsEventReportingDestinationAttestedForSharedStorageMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kAttestationFailed)}});
}

TEST_P(PrivacySandboxAttestationsTest, FledgeAttestationBlockJoiningEtldplus1) {
  GURL top_frame_url("https://top-frame.com");
  GURL enrollee_url("https://embedded.com");
  RunTestCase(
      TestState{
          {MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                             kM1FledgeEnabledUserPrefValue,
                             kM1AdMeasurementEnabledUserPrefValue},
           true},
          {kBlockFledgeJoiningForEtldplus1, std::string("top-frame.com")},
          {kAttestationsMap,
           PrivacySandboxAttestationsMap{
               {net::SchemefulSite(enrollee_url),
                {PrivacySandboxAttestationsGatedAPI::kProtectedAudience}}}}},
      TestInput{
          {kTopicsURL, enrollee_url},
          {kTopFrameOrigin, url::Origin::Create(top_frame_url)},
          {kAdMeasurementReportingOrigin, url::Origin::Create(enrollee_url)},
          {kFledgeAuctionPartyOrigin, url::Origin::Create(enrollee_url)},
          {kEventReportingDestinationOrigin, url::Origin::Create(enrollee_url)},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAccessingOrigin, url::Origin::Create(enrollee_url)}},
      TestOutput{
          {MultipleOutputKeys{kIsEventReportingDestinationAttestedForFledge,
                              kIsFledgeLeaveAllowed, kIsFledgeUpdateAllowed,
                              kIsFledgeSellAllowed, kIsFledgeBuyAllowed},
           true},
          {MultipleOutputKeys{
               kIsFledgeJoinAllowed, kIsTopicsAllowedForContext,
               kIsAttributionReportingAllowed, kMaySendAttributionReport,
               kIsSharedStorageAllowed, kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed,
               kIsEventReportingDestinationAttestedForSharedStorage,
               kIsLocalUnpartitionedDataAccessAllowed},
           false},
          {MultipleOutputKeys{
               kIsFledgeLeaveAllowedMetric, kIsFledgeUpdateAllowedMetric,
               kIsFledgeSellAllowedMetric, kIsFledgeBuyAllowedMetric,
               kIsEventReportingDestinationAttestedForFledgeMetric},
           static_cast<int>(Status::kAllowed)},
          {kIsFledgeJoinAllowedMetric,
           static_cast<int>(Status::kJoiningTopFrameBlocked)},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContextMetric,
               kIsAttributionReportingAllowedMetric,
               kMaySendAttributionReportMetric, kIsSharedStorageAllowedMetric,
               kIsPrivateAggregationAllowedMetric,
               kIsEventReportingDestinationAttestedForSharedStorageMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kAttestationFailed)}});
}

TEST_P(PrivacySandboxAttestationsTest, AttributionReportingAttestation) {
  GURL top_frame_url("https://top-frame.com");
  GURL enrollee_url("https://embedded.com");
  RunTestCase(
      TestState{
          {MultipleStateKeys{kM1TopicsEnabledUserPrefValue,
                             kM1FledgeEnabledUserPrefValue,
                             kM1AdMeasurementEnabledUserPrefValue},
           true},
          {kAttestationsMap,
           PrivacySandboxAttestationsMap{
               {net::SchemefulSite(enrollee_url),
                {PrivacySandboxAttestationsGatedAPI::kAttributionReporting}}}}},
      TestInput{
          {kTopicsURL, enrollee_url},
          {kTopFrameOrigin, url::Origin::Create(top_frame_url)},
          {kAdMeasurementReportingOrigin, url::Origin::Create(enrollee_url)},
          {kFledgeAuctionPartyOrigin, url::Origin::Create(enrollee_url)},
          {kEventReportingDestinationOrigin, url::Origin::Create(enrollee_url)},
          {kAdMeasurementSourceOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAdMeasurementDestinationOrigin,
           url::Origin::Create(GURL(top_frame_url))},
          {kAccessingOrigin, url::Origin::Create(enrollee_url)}},
      TestOutput{
          {MultipleOutputKeys{kIsAttributionReportingAllowed,
                              kMaySendAttributionReport},
           true},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContext, kIsFledgeJoinAllowed,
               kIsFledgeLeaveAllowed, kIsFledgeUpdateAllowed,
               kIsFledgeSellAllowed, kIsFledgeBuyAllowed,
               kIsSharedStorageAllowed,
               kIsEventReportingDestinationAttestedForFledge,
               kIsEventReportingDestinationAttestedForSharedStorage,
               kIsPrivateAggregationAllowed,
               kIsPrivateAggregationDebugModeAllowed,
               kIsLocalUnpartitionedDataAccessAllowed},
           false},
          {MultipleOutputKeys{kIsAttributionReportingAllowedMetric,
                              kMaySendAttributionReportMetric},
           static_cast<int>(Status::kAllowed)},
          {MultipleOutputKeys{
               kIsTopicsAllowedForContextMetric, kIsFledgeJoinAllowedMetric,
               kIsFledgeLeaveAllowedMetric, kIsFledgeUpdateAllowedMetric,
               kIsFledgeSellAllowedMetric, kIsFledgeBuyAllowedMetric,
               kIsSharedStorageAllowedMetric,
               kIsPrivateAggregationAllowedMetric,
               kIsEventReportingDestinationAttestedForSharedStorageMetric,
               kIsEventReportingDestinationAttestedForFledgeMetric,
               kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kAttestationFailed)}});
}

TEST_P(PrivacySandboxAttestationsTest, SetOverrideFromDevtools) {
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));
  privacy_sandbox_settings()->SetAllPrivacySandboxAllowedForTesting();

  // Set an empty attestations map to prevent the API being default allowed
  // when feature `kDefaultAllowPrivacySandboxAttestations` is on.
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAttestationsForTesting(PrivacySandboxAttestationsMap{});

  GURL top_level_url("https://top-level-origin.com");
  GURL caller_url("https://embedded.com");

  // With an empty attestation map, Topics is not allowed.
  EXPECT_FALSE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      url::Origin::Create(top_level_url), caller_url));
  EXPECT_FALSE(privacy_sandbox_settings()->IsEventReportingDestinationAttested(
      url::Origin::Create(GURL("https://embedded.com")),
      privacy_sandbox::PrivacySandboxAttestationsGatedAPI::kProtectedAudience));

  // With an override of the site from a devtools call, Topics is allowed.
  PrivacySandboxAttestations::GetInstance()->AddOverride(
      net::SchemefulSite(GURL("https://embedded.com")));
  EXPECT_TRUE(privacy_sandbox_settings()->IsTopicsAllowedForContext(
      url::Origin::Create(top_level_url), caller_url));
  EXPECT_TRUE(privacy_sandbox_settings()->IsEventReportingDestinationAttested(
      url::Origin::Create(GURL("https://embedded.com")),
      privacy_sandbox::PrivacySandboxAttestationsGatedAPI::kProtectedAudience));
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
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAttestationsForTesting(PrivacySandboxAttestationsMap{});

  for (const auto& test : kTestCases) {
    // Reset the overrides flags from the previous test loop.
    scoped_command_line.GetProcessCommandLine()->RemoveSwitch(
        privacy_sandbox::kPrivacySandboxEnrollmentOverrides);

    // Event reporting for Protected Audience should not be allowed at first.
    EXPECT_FALSE(
        privacy_sandbox_settings()->IsEventReportingDestinationAttested(
            url::Origin::Create(test.report_url),
            privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
                kProtectedAudience));

    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        privacy_sandbox::kPrivacySandboxEnrollmentOverrides, test.flags);

    // Check reporting for Protected Audience after setting the flag.
    EXPECT_EQ(privacy_sandbox_settings()->IsEventReportingDestinationAttested(
                  url::Origin::Create(test.report_url),
                  privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
                      kProtectedAudience),
              test.expected)
        << test.name;
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PrivacySandboxAttestationsTest);

struct PrivacySandbox3pcdTestCase {
  std::string disable_ads_apis_param = "false";
  bool m1_topics_enabled_pref_consent = true;
  bool delegate_experiment_eligibility = true;
  bool output_keys = true;
  int expected_status;
};

const PrivacySandbox3pcdTestCase kTestCases[] = {
    {
        .disable_ads_apis_param = "true",
        .m1_topics_enabled_pref_consent = false,
        .output_keys = false,
        .expected_status =
            /*static_cast<int>(Status::kBlockedBy3pcdExperiment)*/ 11,
    },
    {
        .disable_ads_apis_param = "true",
        .output_keys = false,
        .expected_status =
            /*static_cast<int>(Status::kBlockedBy3pcdExperiment)*/ 11,
    },
    {
        .m1_topics_enabled_pref_consent = false,
        .output_keys = false,
        .expected_status = /*static_cast<int>(Status::kApisDisabled)*/ 3,
    },
    {
        .disable_ads_apis_param = "true",
        .delegate_experiment_eligibility = false,
        .expected_status =
            /*static_cast<int>(Status::kAllowed)*/ 0,
    },
    {
        .expected_status = /*static_cast<int>(Status::kAllowed)*/ 0,
    }};

class PrivacySandbox3pcdExperimentTest
    : public PrivacySandboxSettingsM1Test,
      public testing::WithParamInterface<PrivacySandbox3pcdTestCase> {
 public:
  PrivacySandbox3pcdExperimentTest() = default;

  void InitializeFeaturesBeforeStart() override {
    cookie_deprecation_feature_list_.Reset();
  }

 protected:
  base::test::ScopedFeatureList cookie_deprecation_feature_list_;
};

TEST_P(PrivacySandbox3pcdExperimentTest, ExperimentDisablesAdsAPIs) {
  const PrivacySandbox3pcdTestCase& test_case = GetParam();
  cookie_deprecation_feature_list_.InitAndEnableFeatureWithParameters(
      features::kCookieDeprecationFacilitatedTesting,
      {{features::kCookieDeprecationTestingDisableAdsAPIsName,
        test_case.disable_ads_apis_param}});
  mock_delegate()->SetUpIsCookieDeprecationExperimentEligibleResponse(
      /*eligible=*/test_case.delegate_experiment_eligibility);
  RunTestCase(
      TestState{{kM1TopicsEnabledUserPrefValue,
                 test_case.m1_topics_enabled_pref_consent},
                {kHasAppropriateTopicsConsent,
                 test_case.m1_topics_enabled_pref_consent}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kTopicsURL, GURL("https://embedded.com")},
      },
      TestOutput{
          {MultipleOutputKeys{kIsTopicsAllowed, kIsTopicsAllowedForContext},
           test_case.output_keys},
          {MultipleOutputKeys{
               kIsTopicsAllowedMetric,
               kIsTopicsAllowedForContextMetric,
           },
           test_case.expected_status}});
}

INSTANTIATE_TEST_SUITE_P(PrivacySandbox3pcdExperimentTests,
                         PrivacySandbox3pcdExperimentTest,
                         testing::ValuesIn(kTestCases));

namespace {

constexpr char kAttestationFailedTemplate[] =
    "Attestation check for Shared Storage on %s failed.\nReturned status %d; "
    "see `PrivacySandboxSettingsImpl::Status` at "
    "https://chromium.googlesource.com/chromium/src/+/refs/heads/main/"
    "components/privacy_sandbox/privacy_sandbox_settings_impl.h.";

constexpr char kSiteSettingsTemplate[] =
    "Site access settings returned status %d for accessing origin "
    "%s and top-frame origin %s; see `PrivacySandboxSettingsImpl::Status` at "
    "https://chromium.googlesource.com/chromium/src/+/refs/heads/main/"
    "components/privacy_sandbox/privacy_sandbox_settings_impl.h.";

constexpr char kSandboxRestrictedTemplate[] =
    "Privacy Sandbox settings returned status %d; see "
    "`PrivacySandboxSettingsImpl::Status` at "
    "https://chromium.googlesource.com/chromium/src/+/refs/heads/main/"
    "components/privacy_sandbox/privacy_sandbox_settings_impl.h.";

constexpr char kSelectUrlTemplate[] =
    "M1 measurement settings returned status %d for accessing origin "
    "%s and top-frame origin %s; see `PrivacySandboxSettingsImpl::Status` at "
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
    privacy_sandbox::PrivacySandboxAttestations::GetInstance()
        ->SetAllPrivacySandboxAttestedForTesting(false);
  }

 protected:
  std::string actual_out_shared_storage_debug_message_;
  std::string actual_out_select_url_debug_message_;

 private:
  base::test::ScopedFeatureList default_allow_attestations_feature_list_;
};

}  // namespace

TEST_F(PrivacySandboxSettingsSharedStorageDebugTest, ApiPreferenceEnabled) {
  std::string expected_out_shared_storage_debug_message = base::StringPrintf(
      kSiteSettingsTemplate, static_cast<int>(Status::kAllowed),
      "https://embedded.com", "https://top-frame.com");
  std::string expected_out_select_url_debug_message =
      base::StringPrintf(kSelectUrlTemplate, static_cast<int>(Status::kAllowed),
                         "https://embedded.com", "https://top-frame.com");

  // Confirm that the expected debug messages are received when `sharedStorage`
  // and `sharedStorage.selectURL()` are enabled.
  RunTestCase(
      TestState{{kM1FledgeEnabledUserPrefValue, true}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kAccessingOrigin, url::Origin::Create(GURL("https://embedded.com"))},
          {kOutSharedStorageDebugMessage,
           &actual_out_shared_storage_debug_message_},
          {kOutSharedStorageSelectURLDebugMessage,
           &actual_out_select_url_debug_message_},

          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_},
          {kOutSharedStorageSelectURLBlockIsSiteSettingSpecific,
           &actual_out_select_url_block_is_site_setting_specific_}},
      TestOutput{
          {MultipleOutputKeys{kIsSharedStorageAllowed,
                              kIsSharedStorageSelectURLAllowed,
                              kIsLocalUnpartitionedDataAccessAllowed},
           true},
          {MultipleOutputKeys{kIsSharedStorageAllowedMetric,
                              kIsSharedStorageSelectURLAllowedMetric,
                              kIsLocalUnpartitionedDataAccessAllowedMetric},
           static_cast<int>(Status::kAllowed)},
          {kIsSharedStorageAllowedDebugMessage,
           &expected_out_shared_storage_debug_message},
          {kIsSharedStorageSelectURLAllowedDebugMessage,
           &expected_out_select_url_debug_message},
          {MultipleOutputKeys{
               kIsSharedStorageBlockSiteSettingSpecific,
               kIsSharedStorageSelectURLBlockSiteSettingSpecific},
           &kFalse_}});
}

TEST_F(PrivacySandboxSettingsSharedStorageDebugTest, ApiPreferenceDisabled) {
  std::string expected_out_shared_storage_debug_message = base::StringPrintf(
      kSiteSettingsTemplate, static_cast<int>(Status::kAllowed),
      "https://embedded.com", "https://top-frame.com");
  std::string expected_out_select_url_debug_message = base::StringPrintf(
      kSelectUrlTemplate, static_cast<int>(Status::kApisDisabled),
      "https://embedded.com", "https://top-frame.com");

  // Confirm that the expected debug messages are received when `sharedStorage`
  // is enabled but `sharedStorage.selectURL()` is disabled.
  RunTestCase(
      TestState{{kM1FledgeEnabledUserPrefValue, false}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kAccessingOrigin, url::Origin::Create(GURL("https://embedded.com"))},
          {kOutSharedStorageDebugMessage,
           &actual_out_shared_storage_debug_message_},
          {kOutSharedStorageSelectURLDebugMessage,
           &actual_out_select_url_debug_message_},

          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_},
          {kOutSharedStorageSelectURLBlockIsSiteSettingSpecific,
           &actual_out_select_url_block_is_site_setting_specific_}},
      TestOutput{
          {kIsSharedStorageAllowed, true},
          {kIsSharedStorageSelectURLAllowed, false},
          {kIsSharedStorageAllowedMetric, static_cast<int>(Status::kAllowed)},
          {kIsSharedStorageSelectURLAllowedMetric,
           static_cast<int>(Status::kApisDisabled)},
          {kIsSharedStorageAllowedDebugMessage,
           &expected_out_shared_storage_debug_message},
          {kIsSharedStorageSelectURLAllowedDebugMessage,
           &expected_out_select_url_debug_message},
          {MultipleOutputKeys{
               kIsSharedStorageBlockSiteSettingSpecific,
               kIsSharedStorageSelectURLBlockSiteSettingSpecific},
           &kFalse_}});
}

TEST_F(PrivacySandboxSettingsSharedStorageDebugTest,
       ApisAreOffForRestrictedAccounts) {
  std::string expected_out_shared_storage_debug_message = base::StringPrintf(
      kSandboxRestrictedTemplate, static_cast<int>(Status::kRestricted));
  std::string expected_out_select_url_debug_message = base::StringPrintf(
      kSelectUrlTemplate, static_cast<int>(Status::kRestricted),
      "https://embedded.com", "https://top-frame.com");

  // Confirm that the expected debug messages are received for a restricted
  // account.
  RunTestCase(
      TestState{{MultipleStateKeys{kM1FledgeEnabledUserPrefValue,
                                   kIsRestrictedAccount},
                 true}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kAccessingOrigin, url::Origin::Create(GURL("https://embedded.com"))},
          {kOutSharedStorageDebugMessage,
           &actual_out_shared_storage_debug_message_},
          {kOutSharedStorageSelectURLDebugMessage,
           &actual_out_select_url_debug_message_},

          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_},
          {kOutSharedStorageSelectURLBlockIsSiteSettingSpecific,
           &actual_out_select_url_block_is_site_setting_specific_}},
      TestOutput{{MultipleOutputKeys{kIsSharedStorageAllowed,
                                     kIsSharedStorageSelectURLAllowed},
                  false},
                 {MultipleOutputKeys{kIsSharedStorageAllowedMetric,
                                     kIsSharedStorageSelectURLAllowedMetric},
                  static_cast<int>(Status::kRestricted)},
                 {kIsSharedStorageAllowedDebugMessage,
                  &expected_out_shared_storage_debug_message},
                 {kIsSharedStorageSelectURLAllowedDebugMessage,
                  &expected_out_select_url_debug_message},
                 {MultipleOutputKeys{
                      kIsSharedStorageBlockSiteSettingSpecific,
                      kIsSharedStorageSelectURLBlockSiteSettingSpecific},
                  &kFalse_}});
}

TEST_F(PrivacySandboxSettingsSharedStorageDebugTest,
       SiteDataDefaultBlockExceptionApplies) {
  std::string expected_out_shared_storage_debug_message = base::StringPrintf(
      kSiteSettingsTemplate, static_cast<int>(Status::kSiteDataAccessBlocked),
      "https://embedded.com", "https://top-frame.com");
  std::string expected_out_select_url_debug_message = base::StringPrintf(
      kSelectUrlTemplate, static_cast<int>(Status::kSiteDataAccessBlocked),
      "https://embedded.com", "https://top-frame.com");

  // Confirm that the expected debug messages are received when site settings
  // block exception applies.
  RunTestCase(
      TestState{{kM1FledgeEnabledUserPrefValue, true},
                {kSiteDataUserDefault, CONTENT_SETTING_BLOCK}},
      TestInput{
          {kTopFrameOrigin, url::Origin::Create(GURL("https://top-frame.com"))},
          {kAccessingOrigin, url::Origin::Create(GURL("https://embedded.com"))},
          {kOutSharedStorageDebugMessage,
           &actual_out_shared_storage_debug_message_},
          {kOutSharedStorageSelectURLDebugMessage,
           &actual_out_select_url_debug_message_},

          {kOutSharedStorageBlockIsSiteSettingSpecific,
           &actual_out_shared_storage_block_is_site_setting_specific_},
          {kOutSharedStorageSelectURLBlockIsSiteSettingSpecific,
           &actual_out_select_url_block_is_site_setting_specific_}},
      TestOutput{{MultipleOutputKeys{kIsSharedStorageAllowed,
                                     kIsSharedStorageSelectURLAllowed},
                  false},
                 {MultipleOutputKeys{kIsSharedStorageAllowedMetric,
                                     kIsSharedStorageSelectURLAllowedMetric},
                  static_cast<int>(Status::kSiteDataAccessBlocked)},
                 {kIsSharedStorageAllowedDebugMessage,
                  &expected_out_shared_storage_debug_message},
                 {kIsSharedStorageSelectURLAllowedDebugMessage,
                  &expected_out_select_url_debug_message},
                 {MultipleOutputKeys{
                      kIsSharedStorageBlockSiteSettingSpecific,
                      kIsSharedStorageSelectURLBlockSiteSettingSpecific},
                  &kTrue_}});
}

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

class PrivacySandboxSettingsCookieControlsModeTest
    : public PrivacySandboxSettingsTest {
 public:
  PrivacySandboxSettingsCookieControlsModeTest() {
    feature_list_.InitWithFeatures(
        {privacy_sandbox::kAddLimit3pcsSetting},
        {content_settings::features::kTrackingProtection3pcd});
  }
};

TEST_F(PrivacySandboxSettingsCookieControlsModeTest,
       OnFirstPartySetsEnabledChangedCalledWhen3pcBlockingChanges) {
  prefs()->SetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled, false);

  privacy_sandbox_test_util::MockPrivacySandboxObserver observer;
  privacy_sandbox_settings()->AddObserver(&observer);

  EXPECT_CALL(observer, OnFirstPartySetsEnabledChanged(/*enabled=*/true));
  prefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kLimited));
  testing::Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer, OnFirstPartySetsEnabledChanged(/*enabled=*/false));
  prefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

}  // namespace privacy_sandbox
