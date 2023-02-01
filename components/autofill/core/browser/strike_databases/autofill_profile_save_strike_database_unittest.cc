// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/strike_database_integrator_test_strike_database.h"

#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/browser/strike_databases/autofill_profile_save_strike_database.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {

namespace {

class AutofillProfileSaveStrikeDatabaseTest : public ::testing::Test {
 public:
  AutofillProfileSaveStrikeDatabaseTest() = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());
    strike_database_service_ = std::make_unique<StrikeDatabase>(
        db_provider_.get(), temp_dir_.GetPath());
    strike_database_ = std::make_unique<AutofillProfileSaveStrikeDatabase>(
        strike_database_service_.get());
  }

  void TearDown() override {
    // The destruction of |strike_database_service_|'s components is posted
    // to a task runner, requires running the loop to complete.
    strike_database_.reset();
    strike_database_service_.reset();
    db_provider_.reset();
    task_environment_.RunUntilIdle();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> db_provider_;
  std::unique_ptr<StrikeDatabase> strike_database_service_;
  std::unique_ptr<AutofillProfileSaveStrikeDatabase> strike_database_;

  std::string test_host1 = "https://www.strikedhost.com";
  std::string test_host2 = "https://www.otherhost.com";
  std::string test_host3 = "https://www.justanotherhost.com";

  std::set<std::string> delete_first_host_set = {test_host1};
  std::set<std::string> delete_all_hosts_set = {test_host1, test_host2,
                                                test_host3};
};

TEST_F(AutofillProfileSaveStrikeDatabaseTest, AddAndRemoveStrikes) {
  strike_database_->AddStrike(test_host1);
  EXPECT_EQ(strike_database_->GetStrikes(test_host1), 1);
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(test_host1));

  strike_database_->AddStrikes(2, test_host1);
  EXPECT_EQ(strike_database_->GetStrikes(test_host1), 3);
  EXPECT_TRUE(strike_database_->ShouldBlockFeature(test_host1));

  strike_database_->RemoveStrike(test_host1);
  EXPECT_EQ(strike_database_->GetStrikes(test_host1), 2);
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(test_host1));
}

TEST_F(AutofillProfileSaveStrikeDatabaseTest,
       RemoveStrikesByOriginWithinDeletionWindow) {
  base::Time start_time = AutofillClock::Now();
  // Both strikes are added within the deletion window, but the second should
  // be ruled out by the filter.
  strike_database_->AddStrike(test_host1);
  strike_database_->AddStrike(test_host2);
  base::Time end_time = AutofillClock::Now();

  EXPECT_EQ(strike_database_->GetStrikes(test_host1), 1);
  EXPECT_EQ(strike_database_->GetStrikes(test_host2), 1);

  strike_database_->ClearStrikesByOriginAndTimeInternal(delete_first_host_set,
                                                        start_time, end_time);

  EXPECT_EQ(strike_database_->GetStrikes(test_host1), 0);
  EXPECT_EQ(strike_database_->GetStrikes(test_host2), 1);
}

TEST_F(AutofillProfileSaveStrikeDatabaseTest, RemoveStrikesByOrigin) {
  strike_database_->AddStrike(test_host1);
  strike_database_->AddStrike(test_host2);

  EXPECT_EQ(strike_database_->GetStrikes(test_host1), 1);
  EXPECT_EQ(strike_database_->GetStrikes(test_host2), 1);

  strike_database_->ClearStrikesByOrigin(delete_first_host_set);

  EXPECT_EQ(strike_database_->GetStrikes(test_host1), 0);
  EXPECT_EQ(strike_database_->GetStrikes(test_host2), 1);
}

TEST_F(AutofillProfileSaveStrikeDatabaseTest,
       DoNotRemoveStrikeAfterDeletionWindow) {
  TestAutofillClock test_autofill_clock;
  test_autofill_clock.SetNow(AutofillClock::Now());

  base::Time start_time = AutofillClock::Now();
  strike_database_->AddStrike(test_host1);
  test_autofill_clock.Advance(base::Minutes(1));
  base::Time end_time = AutofillClock::Now();
  test_autofill_clock.Advance(base::Minutes(1));

  // Now update the time stamp of this entry by adding another strike.
  // By this, the entry should not be deleted.
  strike_database_->AddStrike(test_host1);

  strike_database_->ClearStrikesByOriginAndTimeInternal(delete_all_hosts_set,
                                                        start_time, end_time);
  EXPECT_EQ(strike_database_->GetStrikes(test_host1), 2);
}

TEST_F(AutofillProfileSaveStrikeDatabaseTest,
       DoNotRemoveStrikeBeforeDeletionWindow) {
  // The strike is added before the deletion window.
  TestAutofillClock test_autofill_clock;
  test_autofill_clock.SetNow(AutofillClock::Now());

  strike_database_->AddStrike(test_host1);
  test_autofill_clock.Advance(base::Minutes(1));

  base::Time start_time = AutofillClock::Now();
  test_autofill_clock.Advance(base::Minutes(1));
  base::Time end_time = AutofillClock::Now();

  strike_database_->ClearStrikesByOriginAndTimeInternal(delete_all_hosts_set,
                                                        start_time, end_time);

  EXPECT_EQ(strike_database_->GetStrikes(test_host1), 1);
}

}  // namespace

}  // namespace autofill
