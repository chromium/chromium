// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accuracy_tips/accuracy_service.h"
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
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

  MOCK_METHOD3(ShowAccuracyTip,
               void(content::WebContents*,
                    AccuracyTipStatus,
                    base::OnceCallback<void(AccuracyTipInteraction)>));
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
bool IsOnList(const GURL& url,
              safe_browsing::SafeBrowsingDatabaseManager::Client* client) {
  client->OnCheckUrlForAccuracyTip(true);
  return false;
}

// Handler to simulate URLs that match the local hash but are not on the list.
bool IsLocalMatchButNotOnList(
    const GURL& url,
    safe_browsing::SafeBrowsingDatabaseManager::Client* client) {
  client->OnCheckUrlForAccuracyTip(false);
  return false;
}

// Handler that simulates a click on the opt-out button.
void OptOutClicked(content::WebContents*,
                   AccuracyTipStatus,
                   base::OnceCallback<void(AccuracyTipInteraction)> callback) {
  std::move(callback).Run(AccuracyTipInteraction::kOptOut);
}

// Handler that simulates a click on the learn more button.
void LearnMoreClicked(
    content::WebContents*,
    AccuracyTipStatus,
    base::OnceCallback<void(AccuracyTipInteraction)> callback) {
  std::move(callback).Run(AccuracyTipInteraction::kLearnMore);
}

class AccuracyServiceTest : public ::testing::Test {
 protected:
  AccuracyServiceTest() = default;

  void SetUp() override {
    SetUpFeatureList(feature_list_);

    AccuracyService::RegisterProfilePrefs(prefs_.registry());
    auto delegate =
        std::make_unique<testing::StrictMock<MockAccuracyServiceDelegate>>();
    delegate_ = delegate.get();
    EXPECT_CALL(*delegate, IsEngagementHigh(_)).WillRepeatedly(Return(false));

    sb_database_ = base::MakeRefCounted<MockSafeBrowsingDatabaseManager>();
    service_ = std::make_unique<AccuracyService>(
        std::move(delegate), &prefs_, sb_database_,
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

 private:
  virtual void SetUpFeatureList(base::test::ScopedFeatureList& feature_list) {
    feature_list.InitAndEnableFeatureWithParameters(
        safe_browsing::kAccuracyTipsFeature,
        {{features::kSampleUrl.name, "https://sampleurl.com"}});
  }

  base::test::SingleThreadTaskEnvironment environment_;
  base::test::ScopedFeatureList feature_list_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  base::SimpleTestClock clock_;

  MockAccuracyServiceDelegate* delegate_;
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

TEST_F(AccuracyServiceTest, CheckAccuracyStatusForUrlOnList) {
  auto url = GURL("https://badurl.com");
  EXPECT_CALL(*sb_database(), CheckUrlForAccuracyTips(url, _))
      .WillOnce(Invoke(&IsOnList));

  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kShowAccuracyTip);
}

TEST_F(AccuracyServiceTest, CheckAccuracyStatusForLocalMatch) {
  auto url = GURL("https://notactuallybadurl.com");
  EXPECT_CALL(*sb_database(), CheckUrlForAccuracyTips(url, _))
      .WillOnce(Invoke(&IsLocalMatchButNotOnList));

  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kNone);
}

TEST_F(AccuracyServiceTest, ShowUI) {
  EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, _));
  service()->MaybeShowAccuracyTip(nullptr);
}

TEST_F(AccuracyServiceTest, TimeBetweenPrompts) {
  auto url = GURL("https://example.com");
  EXPECT_CALL(*sb_database(), CheckUrlForAccuracyTips(url, _))
      .WillRepeatedly(Invoke(&IsOnList));

  // Show an accuracy tip.
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kShowAccuracyTip);
  EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, _));
  service()->MaybeShowAccuracyTip(nullptr);

  // Future calls will return that the rate limit is active.
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kRateLimited);
  clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kRateLimited);

  // Until sufficient time passed and the tip can be shown again.
  clock()->Advance(features::kTimeBetweenPrompts.Get());
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kShowAccuracyTip);
}

TEST_F(AccuracyServiceTest, OptOut) {
  auto url = GURL("https://example.com");
  EXPECT_CALL(*sb_database(), CheckUrlForAccuracyTips(url, _))
      .WillRepeatedly(Invoke(&IsOnList));

  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kShowAccuracyTip);

  // Clicking the opt-out button will disable future accuracy tips.
  EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, _))
      .WillOnce(Invoke(&OptOutClicked));
  service()->MaybeShowAccuracyTip(nullptr);

  clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kOptOut);

  // Forwarding |kTimeBetweenPrompts| days will also not show the prompt again.
  clock()->Advance(features::kTimeBetweenPrompts.Get());
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kOptOut);
}

TEST_F(AccuracyServiceTest, HighEngagement) {
  auto url = GURL("https://example.com");
  EXPECT_CALL(*sb_database(), CheckUrlForAccuracyTips(url, _))
      .WillRepeatedly(Invoke(&IsOnList));

  // Usually an accuracy tip is shown.
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kShowAccuracyTip);

  // But not if the site has high engagement.
  EXPECT_CALL(*delegate(), IsEngagementHigh(url)).WillRepeatedly(Return(true));
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kHighEnagagement);
}

TEST_F(AccuracyServiceTest, Histograms) {
  {
    base::HistogramTester t;
    EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, _))
        .WillOnce(Invoke(&LearnMoreClicked));
    service()->MaybeShowAccuracyTip(nullptr);
    t.ExpectUniqueSample("Privacy.AccuracyTip.AccuracyTipInteraction",
                         AccuracyTipInteraction::kLearnMore, 1);
    t.ExpectBucketCount("Privacy.AccuracyTip.NumDialogsShown", 1, 1);
    t.ExpectTotalCount("Privacy.AccuracyTip.AccuracyTipTimeOpen", 1);
    t.ExpectBucketCount("Privacy.AccuracyTip.NumDialogsShown.LearnMore", 1, 1);
    t.ExpectTotalCount("Privacy.AccuracyTip.AccuracyTipTimeOpen.LearnMore", 1);
  }

  {
    base::HistogramTester t;
    EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, _))
        .WillOnce(Invoke(&OptOutClicked));
    service()->MaybeShowAccuracyTip(nullptr);
    t.ExpectUniqueSample("Privacy.AccuracyTip.AccuracyTipInteraction",
                         AccuracyTipInteraction::kOptOut, 1);
    t.ExpectBucketCount("Privacy.AccuracyTip.NumDialogsShown", 2, 1);
    t.ExpectTotalCount("Privacy.AccuracyTip.AccuracyTipTimeOpen", 1);
    t.ExpectBucketCount("Privacy.AccuracyTip.NumDialogsShown.OptOut", 2, 1);
    t.ExpectTotalCount("Privacy.AccuracyTip.AccuracyTipTimeOpen.OptOut", 1);
  }
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
  EXPECT_CALL(*delegate(), ShowAccuracyTip(_, _, _)).Times(0);
  service()->MaybeShowAccuracyTip(nullptr);
}

TEST_F(AccuracyServiceDisabledUiTest, TimeBetweenPrompts) {
  auto url = GURL("https://example.com");
  EXPECT_CALL(*sb_database(), CheckUrlForAccuracyTips(url, _))
      .WillRepeatedly(Invoke(&IsOnList));

  // Show an accuracy tip.
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kShowAccuracyTip);
  service()->MaybeShowAccuracyTip(nullptr);

  // Future calls will return that the rate limit is active.
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kRateLimited);
  clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kRateLimited);

  // Until sufficient time passed and the tip can be shown again.
  clock()->Advance(features::kTimeBetweenPrompts.Get());
  EXPECT_EQ(CheckAccuracyStatusSync(url), AccuracyTipStatus::kShowAccuracyTip);
}

}  // namespace accuracy_tips