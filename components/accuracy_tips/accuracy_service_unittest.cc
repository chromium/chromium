// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accuracy_tips/accuracy_service.h"
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/accuracy_tips/accuracy_tip_interaction.h"
#include "components/accuracy_tips/accuracy_tip_status.h"
#include "components/accuracy_tips/features.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "content/public/test/test_renderer_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace accuracy_tips {

class MockAccuracyServiceDelegate : public AccuracyService::Delegate {
 public:
  MockAccuracyServiceDelegate() = default;

  MOCK_METHOD1(IsEngagementHigh, bool(const GURL&));

  MOCK_METHOD4(ShowAccuracyTip,
               void(content::WebContents*,
                    AccuracyTipStatus,
                    bool,
                    base::OnceCallback<void(AccuracyTipInteraction)>));

  MOCK_METHOD2(
      ShowSurvey,
      void(const std::map<std::string, bool>& product_specific_bits_data,
           const std::map<std::string, std::string>&
               product_specific_string_data));

  MOCK_METHOD1(IsSecureConnection, bool(content::WebContents*));
};

class MockSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager()
      : TestSafeBrowsingDatabaseManager(base::ThreadTaskRunnerHandle::Get(),
                                        base::ThreadTaskRunnerHandle::Get()) {}

  MOCK_METHOD2(CheckUrlForAccuracyTips, bool(const GURL&, Client*));

 protected:
  ~MockSafeBrowsingDatabaseManager() override = default;
};

// Handler to mark URLs as part of the AccuracyTips list.
bool IsInList(const GURL& url,
              safe_browsing::SafeBrowsingDatabaseManager::Client* client) {
  client->OnCheckUrlForAccuracyTip(true);
  return false;
}

// Handler to simulate URLs that match the local hash but are not on the list.
bool IsLocalMatchButNotInList(
    const GURL& url,
    safe_browsing::SafeBrowsingDatabaseManager::Client* client) {
  client->OnCheckUrlForAccuracyTip(false);
  return false;
}

// Handler that simulates a click on the opt-out button.
void OptOutClicked(content::WebContents*,
                   AccuracyTipStatus,
                   bool,
                   base::OnceCallback<void(AccuracyTipInteraction)> callback) {
  std::move(callback).Run(AccuracyTipInteraction::kOptOut);
}

// Handler that simulates a click on the learn more button.
void LearnMoreClicked(
    content::WebContents*,
    AccuracyTipStatus,
    bool,
    base::OnceCallback<void(AccuracyTipInteraction)> callback) {
  std::move(callback).Run(AccuracyTipInteraction::kLearnMore);
}

// Handler that simulates a click on the ignore button.
void IgnoreClicked(content::WebContents*,
                   AccuracyTipStatus,
                   bool,
                   base::OnceCallback<void(AccuracyTipInteraction)> callback) {
  std::move(callback).Run(AccuracyTipInteraction::kIgnore);
}

class AccuracyServiceTest : public content::RenderViewHostTestHarness {
 protected:
  AccuracyServiceTest() = default;

  void SetUp() override {
    SetUpFeatureList(feature_list_);
    content::RenderViewHostTestHarness::SetUp();

    AccuracyService::RegisterProfilePrefs(prefs_.registry());
    unified_consent::UnifiedConsentService::RegisterPrefs(prefs_.registry());
    auto delegate =
        std::make_unique<testing::StrictMock<MockAccuracyServiceDelegate>>();
    delegate_ = delegate.get();
    EXPECT_CALL(*delegate, IsEngagementHigh(_)).WillRepeatedly(Return(false));

    sb_database_ = base::MakeRefCounted<MockSafeBrowsingDatabaseManager>();
    service_ = std::make_unique<AccuracyService>(
        std::move(delegate), &prefs_, sb_database_, nullptr,
        base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get());
    clock_.SetNow(base::Time::Now());
    service_->SetClockForTesting(&clock_);
  }

  AccuracyTipStatus CheckAccuracyStatusSync(const GURL& url) {
    base::RunLoop run_loop;
    AccuracyTipStatus status = AccuracyTipStatus::kNone;
    service_->CheckAccuracyStatus(
        url, base::BindLambdaForTesting([&](AccuracyTipStatus s) {
          status = s;
          run_loop.Quit();
        }));
    run_loop.Run();
    return status;
  }

  AccuracyService* service() { return service_.get(); }
  MockAccuracyServiceDelegate* delegate() { return delegate_; }
  base::SimpleTestClock* clock() { return &clock_; }
  MockSafeBrowsingDatabaseManager* sb_database() { return sb_database_.get(); }
  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

 private:
  virtual void SetUpFeatureList(base::test::ScopedFeatureList& feature_list) {
    feature_list.InitAndEnableFeatureWithParameters(
        safe_browsing::kAccuracyTipsFeature,
        {{features::kSampleUrl.name, "https://sampleurl.com"}});
  }

  base::test::ScopedFeatureList feature_list_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  base::SimpleTestClock clock_;

  raw_ptr<MockAccuracyServiceDelegate> delegate_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> sb_database_;
  std::unique_ptr<AccuracyService> service_;
};

TEST_F(AccuracyServiceTest, CheckAccuracyStatusForRandomSite) {
  auto url = GURL("https://example.com");
  EXPECT_CALL(*sb_database(), CheckUrlForAccuracyTips(url, _))
      .WillOnce(Return(true));

  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kNone);
}

TEST_F(AccuracyServiceTest, CheckAccuracyStatusForSampleUrl) {
  auto url = GURL("https://sampleurl.com");
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kShowAccuracyTip);
}

TEST_F(AccuracyServiceTest, CheckAccuracyStatusForUrlInList) {
  auto url = GURL("https://accuracytip.com");
  EXPECT_CALL(*sb_database(), CheckUrlForAccuracyTips(url, _))
      .WillOnce(Invoke(&IsInList));

  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kShowAccuracyTip);
}

TEST_F(AccuracyServiceTest, CheckAccuracyStatusForLocalMatch) {
  auto url = GURL("https://notactuallyaccuracytip.com");
  EXPECT_CALL(*sb_database(), CheckUrlForAccuracyTips(url, _))
      .WillOnce(Invoke(&IsLocalMatchButNotInList));

  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kNone);
}

TEST_F(AccuracyServiceTest, ShowUI) {
  EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, _, _));
  service()->MaybeShowAccuracyTip(web_contents());
}

TEST_F(AccuracyServiceTest, IgnoreButton) {
  EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, false, _))
      .WillOnce(Invoke(&IgnoreClicked));
  service()->MaybeShowAccuracyTip(web_contents());
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, false, _))
      .WillOnce(Invoke(&IgnoreClicked));
  service()->MaybeShowAccuracyTip(web_contents());
  testing::Mock::VerifyAndClearExpectations(delegate());

  EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, true, _))
      .WillOnce(Invoke(&LearnMoreClicked));
  service()->MaybeShowAccuracyTip(web_contents());
  testing::Mock::VerifyAndClearExpectations(delegate());
}

TEST_F(AccuracyServiceTest, TimeBetweenPrompts) {
  auto url = GURL("https://example.com");
  EXPECT_CALL(*sb_database(), CheckUrlForAccuracyTips(url, _))
      .WillRepeatedly(Invoke(&IsInList));

  // Show an accuracy tip.
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kShowAccuracyTip);
  EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, _, _));
  service()->MaybeShowAccuracyTip(web_contents());

  // Future calls will return that the rate limit is active.
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kRateLimited);
  clock()->Advance(base::Days(1));
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kRateLimited);

  // Until sufficient time passed and the tip can be shown again.
  clock()->Advance(features::kTimeBetweenPrompts.Get());
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kShowAccuracyTip);
}

TEST_F(AccuracyServiceTest, OptOut) {
  auto url = GURL("https://example.com");
  EXPECT_CALL(*sb_database(), CheckUrlForAccuracyTips(url, _))
      .WillRepeatedly(Invoke(&IsInList));

  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kShowAccuracyTip);

  // Clicking the opt-out button will disable future accuracy tips.
  EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, _, _))
      .WillOnce(Invoke(&OptOutClicked));
  service()->MaybeShowAccuracyTip(web_contents());

  clock()->Advance(base::Days(1));
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kOptOut);

  // Forwarding |kTimeBetweenPrompts| days will also not show the prompt again.
  clock()->Advance(features::kTimeBetweenPrompts.Get());
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kOptOut);
}

TEST_F(AccuracyServiceTest, HighEngagement) {
  auto url = GURL("https://example.com");
  EXPECT_CALL(*sb_database(), CheckUrlForAccuracyTips(url, _))
      .WillRepeatedly(Invoke(&IsInList));

  // Usually an accuracy tip is shown.
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kShowAccuracyTip);

  // But not if the site has high engagement.
  EXPECT_CALL(*delegate(), IsEngagementHigh(url)).WillRepeatedly(Return(true));
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kHighEnagagement);
}

TEST_F(AccuracyServiceTest, UmaHistograms) {
  {
    base::HistogramTester t;
    EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, _, _))
        .WillOnce(Invoke(&LearnMoreClicked));
    service()->MaybeShowAccuracyTip(web_contents());
    t.ExpectUniqueSample("Privacy.AccuracyTip.AccuracyTipInteraction",
                         AccuracyTipInteraction::kLearnMore, 1);
    t.ExpectBucketCount("Privacy.AccuracyTip.NumDialogsShown", 1, 1);
    t.ExpectTotalCount("Privacy.AccuracyTip.AccuracyTipTimeOpen", 1);
    t.ExpectBucketCount("Privacy.AccuracyTip.NumDialogsShown.LearnMore", 1, 1);
    t.ExpectTotalCount("Privacy.AccuracyTip.AccuracyTipTimeOpen.LearnMore", 1);
  }

  {
    base::HistogramTester t;
    EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, _, _))
        .WillOnce(Invoke(&OptOutClicked));
    service()->MaybeShowAccuracyTip(web_contents());
    t.ExpectUniqueSample("Privacy.AccuracyTip.AccuracyTipInteraction",
                         AccuracyTipInteraction::kOptOut, 1);
    t.ExpectBucketCount("Privacy.AccuracyTip.NumDialogsShown", 2, 1);
    t.ExpectTotalCount("Privacy.AccuracyTip.AccuracyTipTimeOpen", 1);
    t.ExpectBucketCount("Privacy.AccuracyTip.NumDialogsShown.OptOut", 2, 1);
    t.ExpectTotalCount("Privacy.AccuracyTip.AccuracyTipTimeOpen.OptOut", 1);
  }
}

TEST_F(AccuracyServiceTest, UkmHistograms_LearnMore) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, _, _))
      .WillOnce(Invoke(&LearnMoreClicked));
  service()->MaybeShowAccuracyTip(web_contents());

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AccuracyTipDialog::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::AccuracyTipDialog::kInteractionName,
      static_cast<int>(AccuracyTipInteraction::kLearnMore));
}

TEST_F(AccuracyServiceTest, UkmHistograms_OptOut) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, _, _))
      .WillOnce(Invoke(&OptOutClicked));
  service()->MaybeShowAccuracyTip(web_contents());

  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AccuracyTipDialog::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::AccuracyTipDialog::kInteractionName,
      static_cast<int>(AccuracyTipInteraction::kOptOut));
}

class AccuracyServiceDisabledUiTest : public AccuracyServiceTest {
 private:
  void SetUpFeatureList(base::test::ScopedFeatureList& feature_list) override {
    feature_list.InitAndEnableFeatureWithParameters(
        safe_browsing::kAccuracyTipsFeature,
        {{features::kDisableUi.name, "true"}});
  }
};

TEST_F(AccuracyServiceDisabledUiTest, ShowWithUiDisabled) {
  EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, _, _)).Times(0);
  service()->MaybeShowAccuracyTip(web_contents());
}

TEST_F(AccuracyServiceDisabledUiTest, TimeBetweenPrompts) {
  auto url = GURL("https://example.com");
  EXPECT_CALL(*sb_database(), CheckUrlForAccuracyTips(url, _))
      .WillRepeatedly(Invoke(&IsInList));

  // Show an accuracy tip.
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kShowAccuracyTip);
  service()->MaybeShowAccuracyTip(web_contents());

  // Future calls will return that the rate limit is active.
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kRateLimited);
  clock()->Advance(base::Days(1));
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kRateLimited);

  // Until sufficient time passed and the tip can be shown again.
  clock()->Advance(features::kTimeBetweenPrompts.Get());
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kShowAccuracyTip);
}

class AccuracyServiceSurveyTest : public AccuracyServiceTest {
 public:
  void SetUp() override {
    AccuracyServiceTest::SetUp();
    prefs()->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  }

  void ShowAccuracyTipsEnoughTimes() {
    NavigateAndCommit(GURL(gurl_));
    // Before a tip is shown, a survey won't be shown.
    EXPECT_CALL(*delegate(), ShowSurvey(_, _)).Times(0);
    service()->MaybeShowSurvey();
    testing::Mock::VerifyAndClearExpectations(delegate());

    // Show an accuracy tip required number of times.
    for (int i = 0; i < features::kMinPromptCountRequiredForSurvey.Get(); i++) {
      EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, _, _))
          .WillOnce(Invoke(&LearnMoreClicked));
      service()->MaybeShowAccuracyTip(web_contents());

      // Before the tip was shown the required number of times...
      if (i < features::kMinPromptCountRequiredForSurvey.Get() - 1) {
        // ...even though the minimal time has passed...
        clock()->Advance(features::kMinTimeToShowSurvey.Get());
        // ...the survey won't be shown yet.
        EXPECT_CALL(*delegate(), ShowSurvey(_, _)).Times(0);
        service()->MaybeShowSurvey();
        testing::Mock::VerifyAndClearExpectations(delegate());
        clock()->Advance(features::kTimeBetweenPrompts.Get());
      }
    }
  }

 protected:
  GURL gurl_ = GURL("https://sampleurl.com");

 private:
  void SetUpFeatureList(base::test::ScopedFeatureList& feature_list) override {
    const base::FieldTrialParams accuraty_tips_params = {
        {features::kSampleUrl.name, "https://sampleurl.com"}};
    const base::FieldTrialParams accuraty_survey_params = {
        {features::kMinPromptCountRequiredForSurvey.name, "2"}};
    feature_list.InitWithFeaturesAndParameters(
        {{safe_browsing::kAccuracyTipsFeature, accuraty_tips_params},
         {features::kAccuracyTipsSurveyFeature, accuraty_survey_params}},
        {});
  }
};

TEST_F(AccuracyServiceSurveyTest, SurveyTimeRange) {
  ShowAccuracyTipsEnoughTimes();

  // But even after it was shown enough times, need to wait minimal amount of
  // time to show a survey.
  EXPECT_CALL(*delegate(), ShowSurvey(_, _)).Times(0);
  service()->MaybeShowSurvey();
  testing::Mock::VerifyAndClearExpectations(delegate());

  std::map<std::string, std::string> expected_product_specific_data = {
      {"Tip shown for URL", gurl_.DeprecatedGetOriginAsURL().spec()},
      {"UI interaction", base::NumberToString(static_cast<int>(
                             AccuracyTipInteraction::kLearnMore))}};

  // After minimal time passed, a survey can be shown.
  clock()->Advance(features::kMinTimeToShowSurvey.Get());
  EXPECT_CALL(*delegate(), ShowSurvey(_, expected_product_specific_data))
      .Times(1);
  service()->MaybeShowSurvey();
  testing::Mock::VerifyAndClearExpectations(delegate());

  // A survey can be shown in the time range, defined in feature params. After
  // max time passed, a survey cannot be shown anymore.
  clock()->Advance(features::kMaxTimeToShowSurvey.Get());
  EXPECT_CALL(*delegate(), ShowSurvey(_, _)).Times(0);
  service()->MaybeShowSurvey();
  testing::Mock::VerifyAndClearExpectations(delegate());
}

TEST_F(AccuracyServiceSurveyTest, SurveyUkmDisabled) {
  prefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);

  ShowAccuracyTipsEnoughTimes();

  // But even after it was shown enough times, need to wait minimal amount of
  // time to show a survey.
  EXPECT_CALL(*delegate(), ShowSurvey(_, _)).Times(0);
  service()->MaybeShowSurvey();
  testing::Mock::VerifyAndClearExpectations(delegate());

  std::map<std::string, std::string> expected_product_specific_data = {
      {"Tip shown for URL", ""},
      {"UI interaction", base::NumberToString(static_cast<int>(
                             AccuracyTipInteraction::kLearnMore))}};

  // After minimal time passed, a survey can be shown.
  clock()->Advance(features::kMinTimeToShowSurvey.Get());
  EXPECT_CALL(*delegate(), ShowSurvey(_, expected_product_specific_data))
      .Times(1);
  service()->MaybeShowSurvey();
  testing::Mock::VerifyAndClearExpectations(delegate());
}

TEST_F(AccuracyServiceSurveyTest, DontShowSurveyAfterDeletingAllHistory) {
  ShowAccuracyTipsEnoughTimes();

  // All history was deleted...
  service()->OnURLsDeleted(nullptr, history::DeletionInfo::ForAllHistory());
  // ...and even though all other conditions apply, a survey can't be shown
  // because all history was deleted.
  clock()->Advance(features::kMinTimeToShowSurvey.Get());
  EXPECT_CALL(*delegate(), ShowSurvey(_, _)).Times(0);
  service()->MaybeShowSurvey();
  testing::Mock::VerifyAndClearExpectations(delegate());
}

TEST_F(AccuracyServiceSurveyTest, DontShowSurveyAfterDeletingHistoryForUrls) {
  ShowAccuracyTipsEnoughTimes();

  // History for the origin was deleted...
  history::DeletionInfo deletion_info = history::DeletionInfo::ForUrls(
      {history::URLRow(gurl_)}, std::set<GURL>());
  deletion_info.set_deleted_urls_origin_map({
      {gurl_.DeprecatedGetOriginAsURL(), {0, base::Time::Now()}},
  });
  service()->OnURLsDeleted(nullptr, deletion_info);
  // ...and even though all other conditions apply, a survey can't be shown
  // because the relevant history was deleted.
  clock()->Advance(features::kMinTimeToShowSurvey.Get());
  EXPECT_CALL(*delegate(), ShowSurvey(_, _)).Times(0);
  service()->MaybeShowSurvey();
  testing::Mock::VerifyAndClearExpectations(delegate());
}

TEST_F(AccuracyServiceSurveyTest,
       DontShowSurveyAfterDeletingHistoryForTimeRange) {
  ShowAccuracyTipsEnoughTimes();
  clock()->Advance(features::kMinTimeToShowSurvey.Get());

  // History deleted for the last day...
  base::Time begin = clock()->Now() - base::Days(1);
  base::Time end = clock()->Now();
  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(begin, end), false /* is_from_expiration */,
      {} /* deleted_rows */, {} /* favicon_urls */,
      absl::nullopt /* restrict_urls */);
  service()->OnURLsDeleted(nullptr, deletion_info);
  // ...and even though all other conditions apply, a survey can't be shown
  // because the relevant history was deleted.
  EXPECT_CALL(*delegate(), ShowSurvey(_, _)).Times(0);
  service()->MaybeShowSurvey();
  testing::Mock::VerifyAndClearExpectations(delegate());
}

TEST_F(AccuracyServiceSurveyTest, ShowSurveyAfterDeletingHistoryForOtherUrls) {
  ShowAccuracyTipsEnoughTimes();

  // History was deleted for URLs that don't include accuracy tip URL.
  GURL other_gurl = GURL("https://otherurl.com");
  history::DeletionInfo deletion_info = history::DeletionInfo::ForUrls(
      {history::URLRow(other_gurl)}, std::set<GURL>());
  deletion_info.set_deleted_urls_origin_map({
      {other_gurl.DeprecatedGetOriginAsURL(), {0, base::Time::Now()}},
  });
  service()->OnURLsDeleted(nullptr, deletion_info);

  std::map<std::string, std::string> expected_product_specific_data = {
      {"Tip shown for URL", gurl_.DeprecatedGetOriginAsURL().spec()},
      {"UI interaction", base::NumberToString(static_cast<int>(
                             AccuracyTipInteraction::kLearnMore))}};

  // A survey can be shown because history for the accuracy tip URL wasn't
  // deleted.
  clock()->Advance(features::kMinTimeToShowSurvey.Get());
  EXPECT_CALL(*delegate(), ShowSurvey(_, expected_product_specific_data))
      .Times(1);
  service()->MaybeShowSurvey();
  testing::Mock::VerifyAndClearExpectations(delegate());
}

}  // namespace accuracy_tips
