// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/strike_database_integrator_test_strike_database.h"

#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/browser/strike_databases/strike_database_integrator_base.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class StrikeDatabaseIntegratorTestStrikeDatabaseTest : public ::testing::Test {
 public:
  StrikeDatabaseIntegratorTestStrikeDatabaseTest() = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());

    strike_database_service_ = std::make_unique<StrikeDatabase>(
        db_provider_.get(), temp_dir_.GetPath());
    strike_database_ =
        std::make_unique<StrikeDatabaseIntegratorTestStrikeDatabase>(
            strike_database_service_.get());
    no_expiry_strike_database_ =
        std::make_unique<StrikeDatabaseIntegratorTestStrikeDatabase>(
            strike_database_service_.get(), std::nullopt);
  }

  void TearDown() override {
    // The destruction of |strike_database_service_|'s components is posted
    // to a task runner, requires running the loop to complete.
    no_expiry_strike_database_.reset();
    strike_database_.reset();
    strike_database_service_.reset();
    db_provider_.reset();
    task_environment_.RunUntilIdle();
  }

 protected:
  base::HistogramTester* GetHistogramTester() { return &histogram_tester_; }
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> db_provider_;
  std::unique_ptr<StrikeDatabase> strike_database_service_;
  std::unique_ptr<StrikeDatabaseIntegratorTestStrikeDatabase> strike_database_;
  std::unique_ptr<StrikeDatabaseIntegratorTestStrikeDatabase>
      no_expiry_strike_database_;

 private:
  base::HistogramTester histogram_tester_;
};

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       MaxStrikesLimitReachedTest) {
  EXPECT_EQ(StrikeDatabaseIntegratorBase::kDoNotBlock,
            strike_database_->GetStrikeDatabaseDecision());
  // 3 strikes added.
  strike_database_->AddStrikes(3);
  EXPECT_EQ(StrikeDatabaseIntegratorBase::kDoNotBlock,
            strike_database_->GetStrikeDatabaseDecision());
  // 4 strikes added, total strike count is 7.
  strike_database_->AddStrikes(4);
  EXPECT_EQ(StrikeDatabaseIntegratorBase::kMaxStrikeLimitReached,
            strike_database_->GetStrikeDatabaseDecision());
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       StrikeDatabaseIntegratorTestNthStrikeAddedHistogram) {
  // 2 strikes logged.
  strike_database_->AddStrikes(2);
  strike_database_->RemoveStrikes(2);
  // 1 strike logged.
  strike_database_->AddStrike();
  // 2 strikes logged.
  strike_database_->AddStrike();
  std::vector<base::Bucket> buckets = GetHistogramTester()->GetAllSamples(
      "Autofill.StrikeDatabase.NthStrikeAdded.StrikeDatabaseIntegratorTest");
  // There should be two buckets, for strike counts of 1 and 2.
  ASSERT_EQ(2U, buckets.size());
  // Bucket for 1 strike should have count of 1.
  EXPECT_EQ(1, buckets[0].count);
  // Bucket for 2 strikes should have count of 2.
  EXPECT_EQ(2, buckets[1].count);
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       AddStrikeForZeroAndNonZeroStrikesTest) {
  EXPECT_EQ(0, strike_database_->GetStrikes());
  strike_database_->AddStrike();
  EXPECT_EQ(1, strike_database_->GetStrikes());
  strike_database_->AddStrikes(2);
  EXPECT_EQ(3, strike_database_->GetStrikes());
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       ClearStrikesForNonZeroStrikesTest) {
  strike_database_->AddStrikes(3);
  EXPECT_EQ(3, strike_database_->GetStrikes());
  strike_database_->ClearStrikes();
  EXPECT_EQ(0, strike_database_->GetStrikes());
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       ClearStrikesForZeroStrikesTest) {
  strike_database_->ClearStrikes();
  EXPECT_EQ(0, strike_database_->GetStrikes());
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       NonExpiringStrikesDoNotExpire) {
  no_expiry_strike_database_->AddStrikes(1);
  EXPECT_EQ(1, no_expiry_strike_database_->GetStrikes());

  // Advance clock very far into the future.
  task_environment_.FastForwardBy(base::Days(365) * 30);

  no_expiry_strike_database_->RemoveExpiredStrikes();

  // Strike should not be removed.
  EXPECT_EQ(1, no_expiry_strike_database_->GetStrikes());
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       RemoveExpiredStrikesTest) {
  strike_database_->AddStrikes(2);
  EXPECT_EQ(2, strike_database_->GetStrikes());

  // Advance clock to past expiry time.
  task_environment_.FastForwardBy(
      strike_database_->GetExpiryTimeDelta().value() + base::Microseconds(1));

  // One strike should be removed.
  strike_database_->RemoveExpiredStrikes();
  EXPECT_EQ(1, strike_database_->GetStrikes());

  // Strike count is past the max limit.
  strike_database_->AddStrikes(10);
  EXPECT_EQ(11, strike_database_->GetStrikes());

  // Advance clock to past expiry time.
  task_environment_.FastForwardBy(
      strike_database_->GetExpiryTimeDelta().value() + base::Microseconds(1));

  // Strike count should be one less than the max limit.
  strike_database_->RemoveExpiredStrikes();
  EXPECT_EQ(5, strike_database_->GetStrikes());
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       RemoveExpiredStrikesTestLogsUMA) {
  strike_database_->AddStrikes(2);
  EXPECT_EQ(2, strike_database_->GetStrikes());

  // Advance clock to past expiry time.
  task_environment_.FastForwardBy(
      strike_database_->GetExpiryTimeDelta().value() + base::Microseconds(1));

  // One strike should be removed.
  strike_database_->RemoveExpiredStrikes();
  EXPECT_EQ(1, strike_database_->GetStrikes());

  // Strike count is past the max limit.
  strike_database_->AddStrikes(10);
  EXPECT_EQ(11, strike_database_->GetStrikes());

  // Advance clock to past expiry time.
  task_environment_.FastForwardBy(
      strike_database_->GetExpiryTimeDelta().value() + base::Microseconds(1));

  // Strike count should be one less than the max limit.
  strike_database_->RemoveExpiredStrikes();
  EXPECT_EQ(5, strike_database_->GetStrikes());

  std::vector<base::Bucket> buckets = GetHistogramTester()->GetAllSamples(
      "Autofill.StrikeDatabase.StrikesPresentWhenStrikeExpired."
      "StrikeDatabaseIntegratorTest");
  // There should be two buckets, for strike counts of 2 and 11.
  ASSERT_EQ(2U, buckets.size());
  // Bucket for 2 strikes should have count of 1.
  GetHistogramTester()->ExpectBucketCount(
      "Autofill.StrikeDatabase.StrikesPresentWhenStrikeExpired."
      "StrikeDatabaseIntegratorTest",
      2, 1);
  // Bucket for 11 strikes should have count of 1.
  GetHistogramTester()->ExpectBucketCount(
      "Autofill.StrikeDatabase.StrikesPresentWhenStrikeExpired."
      "StrikeDatabaseIntegratorTest",
      11, 1);
}

// This test verifies correctness of http://crbug/1206176.
TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       RemoveExpiredStrikesOnlyConsidersCurrentIntegrator) {
  // Create a second test integrator, but with a different project prefix name,
  // and whose strikes explicitly do not expire.
  std::string other_project_prefix = "DifferentProjectPrefix";
  std::unique_ptr<StrikeDatabaseIntegratorTestStrikeDatabase>
      other_strike_database =
          std::make_unique<StrikeDatabaseIntegratorTestStrikeDatabase>(
              strike_database_service_.get(),
              /*expiry_time_micros=*/std::nullopt, other_project_prefix);

  // Add a strike to both integrators.
  strike_database_->AddStrike();
  EXPECT_EQ(1, strike_database_->GetStrikes());
  other_strike_database->AddStrike();
  EXPECT_EQ(1, other_strike_database->GetStrikes());

  // Advance clock to past expiry time for |strike_database_|.
  task_environment_.FastForwardBy(
      strike_database_->GetExpiryTimeDelta().value() + base::Microseconds(1));

  // Attempt to expire strikes. Only |strike_database_|'s keys should be
  // affected.
  strike_database_->RemoveExpiredStrikes();
  other_strike_database->RemoveExpiredStrikes();
  EXPECT_EQ(0, strike_database_->GetStrikes());
  EXPECT_EQ(1, other_strike_database->GetStrikes());
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       GetKeyForStrikeDatabaseIntegratorUniqueIdTest) {
  strike_database_->SetUniqueIdsRequired(true);
  const std::string unique_id = "1234";
  EXPECT_EQ("StrikeDatabaseIntegratorTest__1234",
            strike_database_->GetKey(unique_id));
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       MaxStrikesLimitReachedUniqueIdTest) {
  strike_database_->SetUniqueIdsRequired(true);
  const std::string unique_id = "1234";
  EXPECT_EQ(StrikeDatabaseIntegratorBase::kDoNotBlock,
            strike_database_->GetStrikeDatabaseDecision(unique_id));
  // 1 strike added for `unique_id`.
  strike_database_->AddStrike(unique_id);
  EXPECT_EQ(StrikeDatabaseIntegratorBase::kDoNotBlock,
            strike_database_->GetStrikeDatabaseDecision(unique_id));
  // 6 strikes added for `unique_id`.
  strike_database_->AddStrikes(6, unique_id);
  EXPECT_EQ(StrikeDatabaseIntegratorBase::kMaxStrikeLimitReached,
            strike_database_->GetStrikeDatabaseDecision(unique_id));
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       StrikeDatabaseIntegratorUniqueIdTestNthStrikeAddedHistogram) {
  strike_database_->SetUniqueIdsRequired(true);
  const std::string unique_id_1 = "1234";
  const std::string unique_id_2 = "9876";
  // 1st strike added for |unique_id_1|.
  strike_database_->AddStrike(unique_id_1);
  // 2nd strike added for |unique_id_1|.
  strike_database_->AddStrike(unique_id_1);
  // 1st strike added for |unique_id_2|.
  strike_database_->AddStrike(unique_id_2);
  std::vector<base::Bucket> buckets = GetHistogramTester()->GetAllSamples(
      "Autofill.StrikeDatabase.NthStrikeAdded."
      "StrikeDatabaseIntegratorTest");
  // There should be two buckets, one for 1st strike, one for 2nd strike count.
  ASSERT_EQ(2U, buckets.size());
  // Both |unique_id_1| and |unique_id_2| have 1st strikes recorded.
  EXPECT_EQ(2, buckets[0].count);
  // Only |unique_id_1| has 2nd strike recorded.
  EXPECT_EQ(1, buckets[1].count);
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       StrikeDatabaseIntegratorUniqueIdTestClearAllStrikes) {
  strike_database_->SetUniqueIdsRequired(true);
  const std::string unique_id_1 = "1234";
  const std::string unique_id_2 = "9876";
  // 1 strike added for |unique_id_1|.
  strike_database_->AddStrike(unique_id_1);
  // 3 strikes added for |unique_id_2|.
  strike_database_->AddStrikes(3, unique_id_2);
  EXPECT_EQ(1, strike_database_->GetStrikes(unique_id_1));
  EXPECT_EQ(3, strike_database_->GetStrikes(unique_id_2));
  strike_database_->ClearAllStrikes();
  EXPECT_EQ(0, strike_database_->GetStrikes(unique_id_1));
  EXPECT_EQ(0, strike_database_->GetStrikes(unique_id_2));
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       AddStrikeForZeroAndNonZeroStrikesUniqueIdTest) {
  strike_database_->SetUniqueIdsRequired(true);
  const std::string unique_id = "1234";
  EXPECT_EQ(0, strike_database_->GetStrikes(unique_id));
  strike_database_->AddStrike(unique_id);
  EXPECT_EQ(1, strike_database_->GetStrikes(unique_id));
  strike_database_->AddStrike(unique_id);
  EXPECT_EQ(2, strike_database_->GetStrikes(unique_id));
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       ClearStrikesForNonZeroStrikesUniqueIdTest) {
  strike_database_->SetUniqueIdsRequired(true);
  const std::string unique_id = "1234";
  strike_database_->AddStrike(unique_id);
  EXPECT_EQ(1, strike_database_->GetStrikes(unique_id));
  strike_database_->ClearStrikes(unique_id);
  EXPECT_EQ(0, strike_database_->GetStrikes(unique_id));
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       ClearStrikesForZeroStrikesUniqueIdTest) {
  strike_database_->SetUniqueIdsRequired(true);
  const std::string unique_id = "1234";
  strike_database_->ClearStrikes(unique_id);
  EXPECT_EQ(0, strike_database_->GetStrikes(unique_id));
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       RemoveExpiredStrikesUniqueIdTest) {
  strike_database_->SetUniqueIdsRequired(true);
  const std::string unique_id_1 = "1234";
  const std::string unique_id_2 = "9876";
  strike_database_->AddStrike(unique_id_1);

  // Advance clock to past the entry for |unique_id_1|'s expiry time.
  task_environment_.FastForwardBy(
      strike_database_->GetExpiryTimeDelta().value() + base::Microseconds(1));

  strike_database_->AddStrike(unique_id_2);
  strike_database_->RemoveExpiredStrikes();

  // |unique_id_1|'s entry should have its most recent strike expire, but
  // |unique_id_2|'s should not.
  EXPECT_EQ(0, strike_database_->GetStrikes(unique_id_1));
  EXPECT_EQ(1, strike_database_->GetStrikes(unique_id_2));

  // Advance clock to past |unique_id_2|'s expiry time.
  task_environment_.FastForwardBy(
      strike_database_->GetExpiryTimeDelta().value() + base::Microseconds(1));

  strike_database_->RemoveExpiredStrikes();

  // |unique_id_1| and |unique_id_2| should have no more unexpired strikes.
  EXPECT_EQ(0, strike_database_->GetStrikes(unique_id_1));
  EXPECT_EQ(0, strike_database_->GetStrikes(unique_id_2));
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest, CountEntries) {
  strike_database_->SetUniqueIdsRequired(true);
  const std::string unique_id_1 = "111";
  const std::string unique_id_2 = "222";
  const std::string unique_id_3 = "333";

  EXPECT_EQ(0U, strike_database_->CountEntries());
  strike_database_->AddStrike(unique_id_1);
  EXPECT_EQ(1U, strike_database_->CountEntries());
  strike_database_->AddStrike(unique_id_1);
  EXPECT_EQ(1U, strike_database_->CountEntries());

  strike_database_->AddStrike(unique_id_2);
  strike_database_->AddStrike(unique_id_3);
  EXPECT_EQ(3U, strike_database_->CountEntries());
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest, ClearStrikesForKeys) {
  strike_database_->SetUniqueIdsRequired(true);
  const std::string unique_id_1 = "111";
  const std::string unique_id_2 = "222";
  const std::string unique_id_3 = "333";

  strike_database_->AddStrike(unique_id_1);
  strike_database_->AddStrike(unique_id_2);
  strike_database_->AddStrike(unique_id_3);
  EXPECT_EQ(3U, strike_database_->CountEntries());

  std::vector<std::string> keys_to_clear = {
      strike_database_->GetKey(unique_id_1),
      strike_database_->GetKey(unique_id_2)};
  strike_database_->ClearStrikesForKeys(keys_to_clear);

  EXPECT_EQ(1U, strike_database_->CountEntries());

  EXPECT_EQ(0, strike_database_->GetStrikes(unique_id_1));
  EXPECT_EQ(0, strike_database_->GetStrikes(unique_id_2));
  EXPECT_EQ(1, strike_database_->GetStrikes(unique_id_3));
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest, IdFromKey) {
  strike_database_->SetUniqueIdsRequired(true);
  const std::string unique_id = "111";
  std::string key = strike_database_->GetKey(unique_id);
  ASSERT_EQ(key, "StrikeDatabaseIntegratorTest__111");
  EXPECT_EQ(unique_id, strike_database_->GetIdFromKey(key));
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       LimitTheNumberOfElements) {
  strike_database_->SetUniqueIdsRequired(true);
  for (size_t i = 1; i <= 10; i++) {
    strike_database_->AddStrike(base::NumberToString(i));
    task_environment_.FastForwardBy(base::Microseconds(1));
    EXPECT_EQ(i, strike_database_->CountEntries());
  }
  // Once the 11th element is added the cleanup should reduce the number of
  // elements to 5. Note that the index here is chosen to be smaller than the
  // previous indices. The purpose is to ensure that the deletion is actually
  // done in the order of time stamp and not index.
  strike_database_->AddStrike(base::NumberToString(0));
  EXPECT_EQ(5U, strike_database_->CountEntries());

  // Verify that the oldest 6 elements have been deleted.
  for (size_t i = 1; i <= 10; i++) {
    EXPECT_EQ(i <= 6 ? 0 : 1,
              strike_database_->GetStrikes(base::NumberToString(i)));
  }
}

// Test to ensure that `GetStrikeDatabaseDecision` function works correctly with
// the required latency since last strike requirement.
TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       HasRequiredDelayPassedSinceLastStrike) {
  strike_database_->SetUniqueIdsRequired(true);
  strike_database_->SetRequiredDelaySinceLastStrike(base::Days(7));

  strike_database_->AddStrike("fake key");
  ASSERT_EQ(1U, strike_database_->CountEntries());

  EXPECT_EQ(StrikeDatabaseIntegratorBase::kRequiredDelayNotPassed,
            strike_database_->GetStrikeDatabaseDecision("fake key"));

  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(StrikeDatabaseIntegratorBase::kRequiredDelayNotPassed,
            strike_database_->GetStrikeDatabaseDecision("fake key"));

  task_environment_.FastForwardBy(base::Days(7));
  EXPECT_EQ(StrikeDatabaseIntegratorBase::kDoNotBlock,
            strike_database_->GetStrikeDatabaseDecision("fake key"));

  strike_database_->AddStrike("fake key");
  EXPECT_EQ(StrikeDatabaseIntegratorBase::kRequiredDelayNotPassed,
            strike_database_->GetStrikeDatabaseDecision("fake key"));
}

// Test to ensure `ShouldBlockFeature` returns correctly based on
// `GetStrikeDatabaseDecision` for the max strikes limit condition.
TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       ShouldBlockFeature_MaxStrikes) {
  EXPECT_FALSE(strike_database_->ShouldBlockFeature());

  // Add max strikes.
  strike_database_->AddStrikes(strike_database_->GetMaxStrikesLimit());
  EXPECT_TRUE(strike_database_->ShouldBlockFeature());
}

// Test to ensure `ShouldBlockFeature` returns correctly based on
// `GetStrikeDatabaseDecision` for the required delay condition.
TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       ShouldBlockFeature_RequiredDelay) {
  strike_database_->SetRequiredDelaySinceLastStrike(base::Days(7));
  EXPECT_FALSE(strike_database_->ShouldBlockFeature());

  // Add a single strike without advancing the clock.
  strike_database_->AddStrike();
  EXPECT_TRUE(strike_database_->ShouldBlockFeature());
}

}  // namespace autofill
