// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/quick_answers_notice.h"

#include <memory>
#include <string>

#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace quick_answers {

class QuickAnswersNoticeTest : public testing::Test {
 public:
  QuickAnswersNoticeTest() = default;

  ~QuickAnswersNoticeTest() override = default;

  void SetUp() override {
    prefs::RegisterProfilePrefs(pref_service_.registry());
    notice_ = std::make_unique<QuickAnswersNotice>(&pref_service_);
  }

  void TearDown() override { notice_.reset(); }

  PrefService* pref_service() { return &pref_service_; }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<QuickAnswersNotice> notice_;
};

TEST_F(QuickAnswersNoticeTest, ShouldShowNoticeShouldBeTrueIfUserHasNoticeed) {
  EXPECT_TRUE(notice_->ShouldShowNotice());

  pref_service()->SetBoolean(prefs::kQuickAnswersConsented, true);

  // Verify that it is consented.
  EXPECT_FALSE(notice_->ShouldShowNotice());
}

TEST_F(QuickAnswersNoticeTest, ShouldShowNoticeHasReachedImpressionCap) {
  EXPECT_TRUE(notice_->ShouldShowNotice());

  pref_service()->SetInteger(prefs::kQuickAnswersNoticeImpressionCount, 3);

  // Verify that impression cap is reached.
  EXPECT_FALSE(notice_->ShouldShowNotice());
}

TEST_F(QuickAnswersNoticeTest, ShouldShowNoticeHasReachedDurationCap) {
  EXPECT_TRUE(notice_->ShouldShowNotice());

  pref_service()->SetInteger(prefs::kQuickAnswersNoticeImpressionDuration, 7);
  // Not reach impression duration cap yet.
  EXPECT_TRUE(notice_->ShouldShowNotice());

  pref_service()->SetInteger(prefs::kQuickAnswersNoticeImpressionDuration, 8);
  // Reach impression duration cap.
  EXPECT_FALSE(notice_->ShouldShowNotice());
}

TEST_F(QuickAnswersNoticeTest, AcceptNotice) {
  EXPECT_TRUE(notice_->ShouldShowNotice());

  notice_->StartNotice();

  // Notice is accepted after 6 seconds.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(6));
  notice_->AcceptNotice(NoticeInteractionType::kAccept);

  // Verify that it is consented.
  ASSERT_TRUE(pref_service()->GetBoolean(prefs::kQuickAnswersConsented));
  // Verify that the duration is recorded.
  ASSERT_EQ(6, pref_service()->GetInteger(
                   prefs::kQuickAnswersNoticeImpressionDuration));
  // Verify that it is consented.
  EXPECT_FALSE(notice_->ShouldShowNotice());
}

TEST_F(QuickAnswersNoticeTest, DismissNotice) {
  // Start consent.
  notice_->StartNotice();

  // Dismiss consent after reaching the impression cap.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(8));
  notice_->DismissNotice();

  // Verify that the impression count is recorded.
  ASSERT_EQ(8, pref_service()->GetInteger(
                   prefs::kQuickAnswersNoticeImpressionDuration));
}

}  // namespace quick_answers
}  // namespace chromeos
