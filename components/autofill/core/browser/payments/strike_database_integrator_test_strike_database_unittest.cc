// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/strike_database_integrator_test_strike_database.h"

#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class StrikeDatabaseIntegratorTestStrikeDatabaseTest : public ::testing::Test {
 public:
  StrikeDatabaseIntegratorTestStrikeDatabaseTest() {}

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());

    strike_database_service_ = std::make_unique<StrikeDatabase>(
        db_provider_.get(), temp_dir_.GetPath());
    strike_database_ =
        std::make_unique<StrikeDatabaseIntegratorTestStrikeDatabase>(
            strike_database_service_.get());
  }

  void TearDown() override {
    // The destruction of |strike_database_service_|'s components is posted
    // to a task runner, requires running the loop to complete.
    strike_database_.reset();
    strike_database_service_.reset();
    task_environment_.RunUntilIdle();
  }

 protected:
  base::HistogramTester* GetHistogramTester() { return &histogram_tester_; }
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> db_provider_;
  std::unique_ptr<StrikeDatabase> strike_database_service_;
  std::unique_ptr<StrikeDatabaseIntegratorTestStrikeDatabase> strike_database_;

 private:
  base::HistogramTester histogram_tester_;
};

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       MaxStrikesLimitReachedTest) {
  EXPECT_EQ(false, strike_database_->IsMaxStrikesLimitReached());
  // 3 strikes added.
  strike_database_->AddStrikes(3);
  EXPECT_EQ(false, strike_database_->IsMaxStrikesLimitReached());
  // 4 strike added, total strike count is 7.
  strike_database_->AddStrikes(4);
  EXPECT_EQ(true, strike_database_->IsMaxStrikesLimitReached());
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
       RemoveExpiredStrikesTest) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(AutofillClock::Now());
  strike_database_->AddStrikes(2);
  EXPECT_EQ(2, strike_database_->GetStrikes());

  // Advance clock to past expiry time.
  test_clock.Advance(base::TimeDelta::FromMicroseconds(
      strike_database_->GetExpiryTimeMicros() + 1));

  // One strike should be removed.
  strike_database_->RemoveExpiredStrikes();
  EXPECT_EQ(1, strike_database_->GetStrikes());

  // Strike count is past the max limit.
  strike_database_->AddStrikes(10);
  EXPECT_EQ(11, strike_database_->GetStrikes());

  // Advance clock to past expiry time.
  test_clock.Advance(base::TimeDelta::FromMicroseconds(
      strike_database_->GetExpiryTimeMicros() + 1));

  // Strike count should be one less than the max limit.
  strike_database_->RemoveExpiredStrikes();
  EXPECT_EQ(5, strike_database_->GetStrikes());
}

TEST_F(StrikeDatabaseIntegratorTestStrikeDatabaseTest,
       RemoveExpiredStrikesTestLogsUMA) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(AutofillClock::Now());
  strike_database_->AddStrikes(2);
  EXPECT_EQ(2, strike_database_->GetStrikes());

  // Advance clock to past expiry time.
  test_clock.Advance(base::TimeDelta::FromMicroseconds(
      strike_database_->GetExpiryTimeMicros() + 1));

  // One strike should be removed.
  strike_database_->RemoveExpiredStrikes();
  EXPECT_EQ(1, strike_database_->GetStrikes());

  // Strike count is past the max limit.
  strike_database_->AddStrikes(10);
  EXPECT_EQ(11, strike_database_->GetStrikes());

  // Advance clock to past expiry time.
  test_clock.Advance(base::TimeDelta::FromMicroseconds(
      strike_database_->GetExpiryTimeMicros() + 1));

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
  EXPECT_EQ(false, strike_database_->IsMaxStrikesLimitReached(unique_id));
  // 1 strike added for |unique_id|.
  strike_database_->AddStrike(unique_id);
  EXPECT_EQ(false, strike_database_->IsMaxStrikesLimitReached(unique_id));
  // 6 strikes added for |unique_id|.
  strike_database_->AddStrikes(6, unique_id);
  EXPECT_EQ(true, strike_database_->IsMaxStrikesLimitReached(unique_id));
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
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(AutofillClock::Now());
  const std::string unique_id_1 = "1234";
  const std::string unique_id_2 = "9876";
  strike_database_->AddStrike(unique_id_1);

  // Advance clock to past the entry for |unique_id_1|'s expiry time.
  test_clock.Advance(base::TimeDelta::FromMicroseconds(
      strike_database_->GetExpiryTimeMicros() + 1));

  strike_database_->AddStrike(unique_id_2);
  strike_database_->RemoveExpiredStrikes();

  // |unique_id_1|'s entry should have its most recent strike expire, but
  // |unique_id_2|'s should not.
  EXPECT_EQ(0, strike_database_->GetStrikes(unique_id_1));
  EXPECT_EQ(1, strike_database_->GetStrikes(unique_id_2));

  // Advance clock to past |unique_id_2|'s expiry time.
  test_clock.Advance(base::TimeDelta::FromMicroseconds(
      strike_database_->GetExpiryTimeMicros() + 1));

  strike_database_->RemoveExpiredStrikes();

  // |unique_id_1| and |unique_id_2| should have no more unexpired strikes.
  EXPECT_EQ(0, strike_database_->GetStrikes(unique_id_1));
  EXPECT_EQ(0, strike_database_->GetStrikes(unique_id_2));
}

}  // namespace autofill
