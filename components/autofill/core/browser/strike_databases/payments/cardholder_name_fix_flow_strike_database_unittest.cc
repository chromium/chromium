// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/payments/cardholder_name_fix_flow_strike_database.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/strike_database/strike_database.h"
#include "components/strike_database/strike_database_integrator_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class CardholderNameFixFlowStrikeDatabaseTest : public ::testing::Test {
 public:
  CardholderNameFixFlowStrikeDatabaseTest() = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());

    strike_database_service_ =
        std::make_unique<strike_database::StrikeDatabase>(db_provider_.get(),
                                                          temp_dir_.GetPath());

    strike_database_ = std::make_unique<CardholderNameFixFlowStrikeDatabase>(
        strike_database_service_.get());
  }

  void TearDown() override {
    strike_database_.reset();
    strike_database_service_.reset();
    db_provider_.reset();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> db_provider_;
  std::unique_ptr<strike_database::StrikeDatabase> strike_database_service_;
  std::unique_ptr<CardholderNameFixFlowStrikeDatabase> strike_database_;
};

TEST_F(CardholderNameFixFlowStrikeDatabaseTest, StrikeDatabaseProperties) {
  EXPECT_EQ(strike_database_->GetMaxStrikesLimit(), 3);
  EXPECT_EQ(strike_database_->GetRequiredDelaySinceLastStrike(), base::Days(7));
  EXPECT_EQ(strike_database_->GetExpiryTimeDelta(), std::nullopt);
  EXPECT_TRUE(strike_database_->UniqueIdsRequired());
}

TEST_F(CardholderNameFixFlowStrikeDatabaseTest, StrikesWorkflow) {
  std::string instrument_id = "123456789";

  EXPECT_EQ(strike_database_->GetStrikes(instrument_id), 0);

  // 1 strike
  EXPECT_EQ(strike_database_->AddStrike(instrument_id), 1);
  EXPECT_EQ(strike_database_->GetStrikes(instrument_id), 1);

  // 2 strikes
  EXPECT_EQ(strike_database_->AddStrike(instrument_id), 2);
  EXPECT_EQ(strike_database_->GetStrikes(instrument_id), 2);

  // Remove 1 strike
  EXPECT_EQ(strike_database_->RemoveStrike(instrument_id), 1);
  EXPECT_EQ(strike_database_->GetStrikes(instrument_id), 1);

  // Clear strikes
  strike_database_->ClearStrikes(instrument_id);
  EXPECT_EQ(strike_database_->GetStrikes(instrument_id), 0);
}

TEST_F(CardholderNameFixFlowStrikeDatabaseTest,
       EnforcedDelayAndMaxStrikeDecision) {
  std::string instrument_id = "123456789";

  // Initially, no strikes, feature should not be blocked.
  EXPECT_EQ(strike_database_->GetStrikeDatabaseDecision(instrument_id),
            CardholderNameFixFlowStrikeDatabase::kDoNotBlock);
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(instrument_id));

  // Add 1 strike. Cooldown of 7 days takes effect immediately.
  strike_database_->AddStrike(instrument_id);
  EXPECT_EQ(strike_database_->GetStrikeDatabaseDecision(instrument_id),
            CardholderNameFixFlowStrikeDatabase::kRequiredDelayNotPassed);
  EXPECT_TRUE(strike_database_->ShouldBlockFeature(instrument_id));

  // Fast forward 6 days (still within the 7-day cooldown).
  task_environment_.FastForwardBy(base::Days(6));
  EXPECT_EQ(strike_database_->GetStrikeDatabaseDecision(instrument_id),
            CardholderNameFixFlowStrikeDatabase::kRequiredDelayNotPassed);
  EXPECT_TRUE(strike_database_->ShouldBlockFeature(instrument_id));

  // Fast forward 1 more day (7 days total elapsed since strike).
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(strike_database_->GetStrikeDatabaseDecision(instrument_id),
            CardholderNameFixFlowStrikeDatabase::kDoNotBlock);
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(instrument_id));

  // Add 2 more strikes (reaching max limit of 3 strikes).
  strike_database_->AddStrikes(2, instrument_id);
  EXPECT_EQ(strike_database_->GetStrikes(instrument_id), 3);
  EXPECT_EQ(strike_database_->GetStrikeDatabaseDecision(instrument_id),
            CardholderNameFixFlowStrikeDatabase::kMaxStrikeLimitReached);
  EXPECT_TRUE(strike_database_->ShouldBlockFeature(instrument_id));

  // Fast forward beyond the cooldown period; it should remain blocked because
  // max strike limit is reached.
  task_environment_.FastForwardBy(base::Days(10));
  EXPECT_EQ(strike_database_->GetStrikeDatabaseDecision(instrument_id),
            CardholderNameFixFlowStrikeDatabase::kMaxStrikeLimitReached);
  EXPECT_TRUE(strike_database_->ShouldBlockFeature(instrument_id));

  // Clearing strikes unblocks the feature.
  strike_database_->ClearStrikes(instrument_id);
  EXPECT_EQ(strike_database_->GetStrikeDatabaseDecision(instrument_id),
            CardholderNameFixFlowStrikeDatabase::kDoNotBlock);
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(instrument_id));
}

}  // namespace

}  // namespace autofill
