// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/strike_databases/ewallet_account_linking_strike_database.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/strike_database/test_inmemory_strike_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {

namespace {

class EwalletAccountLinkingStrikeDatabaseTest : public ::testing::Test {
 public:
  EwalletAccountLinkingStrikeDatabaseTest() = default;

  void SetUp() override {
    strike_database_service_ =
        std::make_unique<strike_database::TestInMemoryStrikeDatabase>();

    strike_database_ = std::make_unique<EwalletAccountLinkingStrikeDatabase>(
        strike_database_service_.get());
  }

  void TearDown() override {
    strike_database_.reset();
    strike_database_service_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<strike_database::TestInMemoryStrikeDatabase>
      strike_database_service_;
  std::unique_ptr<EwalletAccountLinkingStrikeDatabase> strike_database_;
};

TEST_F(EwalletAccountLinkingStrikeDatabaseTest,
       GetMaxStrikesLimit_EnforcesThreeStrikeLimit) {
  EXPECT_EQ(strike_database_->GetMaxStrikesLimit(), 3);
  EXPECT_EQ(strike_database_->GetStrikes(), 0);
  EXPECT_FALSE(strike_database_->ShouldBlockFeature());

  strike_database_->AddStrike();
  strike_database_->AddStrike();
  strike_database_->AddStrike();

  EXPECT_EQ(strike_database_->GetStrikes(), 3);
  EXPECT_TRUE(strike_database_->ShouldBlockFeature());
  EXPECT_EQ(
      strike_database_->GetStrikeDatabaseDecision(),
      strike_database::StrikeDatabaseIntegratorBase::kMaxStrikeLimitReached);
}

TEST_F(EwalletAccountLinkingStrikeDatabaseTest,
       GetRequiredDelaySinceLastStrike_EnforcesSevenDayDelay) {
  EXPECT_EQ(strike_database_->GetRequiredDelaySinceLastStrike(), base::Days(7));

  strike_database_->AddStrike();
  EXPECT_EQ(strike_database_->GetStrikes(), 1);
  EXPECT_TRUE(strike_database_->ShouldBlockFeature());
  EXPECT_EQ(
      strike_database_->GetStrikeDatabaseDecision(),
      strike_database::StrikeDatabaseIntegratorBase::kRequiredDelayNotPassed);

  task_environment_.FastForwardBy(base::Days(6) + base::Hours(23));
  EXPECT_TRUE(strike_database_->ShouldBlockFeature());

  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_FALSE(strike_database_->ShouldBlockFeature());
  EXPECT_EQ(strike_database_->GetStrikeDatabaseDecision(),
            strike_database::StrikeDatabaseIntegratorBase::kDoNotBlock);
}

TEST_F(EwalletAccountLinkingStrikeDatabaseTest, StrikesNeverExpire) {
  strike_database_->AddStrike();
  strike_database_->AddStrike();
  strike_database_->AddStrike();
  EXPECT_EQ(strike_database_->GetStrikes(), 3);
  EXPECT_TRUE(strike_database_->ShouldBlockFeature());

  task_environment_.FastForwardBy(base::Days(365));
  EXPECT_EQ(strike_database_->GetStrikes(), 3);
  EXPECT_TRUE(strike_database_->ShouldBlockFeature());
}

TEST_F(EwalletAccountLinkingStrikeDatabaseTest,
       ClearStrikes_ResetsStrikeCountToZero) {
  strike_database_->AddStrike();
  strike_database_->AddStrike();
  EXPECT_EQ(strike_database_->GetStrikes(), 2);

  strike_database_->ClearStrikes();
  EXPECT_EQ(strike_database_->GetStrikes(), 0);
  EXPECT_FALSE(strike_database_->ShouldBlockFeature());
}

TEST_F(EwalletAccountLinkingStrikeDatabaseTest,
       ClearStrikes_FromMaxStrikes_UnblocksFeature) {
  strike_database_->AddStrike();
  strike_database_->AddStrike();
  strike_database_->AddStrike();
  EXPECT_EQ(strike_database_->GetStrikes(), 3);
  EXPECT_TRUE(strike_database_->ShouldBlockFeature());
  EXPECT_EQ(
      strike_database_->GetStrikeDatabaseDecision(),
      strike_database::StrikeDatabaseIntegratorBase::kMaxStrikeLimitReached);

  strike_database_->ClearStrikes();
  EXPECT_EQ(strike_database_->GetStrikes(), 0);
  EXPECT_FALSE(strike_database_->ShouldBlockFeature());
  EXPECT_EQ(strike_database_->GetStrikeDatabaseDecision(),
            strike_database::StrikeDatabaseIntegratorBase::kDoNotBlock);
}

}  // namespace
}  // namespace payments::facilitated
