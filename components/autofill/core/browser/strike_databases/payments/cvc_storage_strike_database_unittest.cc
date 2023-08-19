// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/strike_database_integrator_test_strike_database.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/browser/strike_databases/payments/cvc_storage_strike_database.h"
#include "components/autofill/core/browser/strike_databases/strike_database_integrator_base.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class CvcStorageStrikeDatabaseTest : public ::testing::Test {
 public:
  CvcStorageStrikeDatabaseTest() = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());

    strike_database_service_ = std::make_unique<StrikeDatabase>(
        db_provider_.get(), temp_dir_.GetPath());

    strike_database_ = std::make_unique<CvcStorageStrikeDatabase>(
        strike_database_service_.get());
  }

  void TearDown() override {
    // The destruction of `strike_database_service_`'s components is posted
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
  std::unique_ptr<CvcStorageStrikeDatabase> strike_database_;
};

TEST_F(CvcStorageStrikeDatabaseTest, AddAndRemoveStrikes) {
  int max_strikes = strike_database_->GetMaxStrikesLimit();
  ASSERT_TRUE(strike_database_->GetRequiredDelaySinceLastStrike().has_value());
  std::string kTestGuid = "00000000-0000-0000-0000-000000000001";
  TestAutofillClock test_autofill_clock(AutofillClock::Now());

  EXPECT_EQ(strike_database_->GetStrikes(kTestGuid), 0);
  // Add one strike for the CVC.
  strike_database_->AddStrike(kTestGuid);
  EXPECT_EQ(strike_database_->GetStrikes(kTestGuid), 1);

  // Verify at this moment, even though the strike limit has not been reached,
  // the feature should still be blocked since it is still within the enforced
  // delay period.
  EXPECT_TRUE(strike_database_->ShouldBlockFeature(kTestGuid));

  // Advance time and verify the feature should not be blocked.
  test_autofill_clock.Advance(
      strike_database_->GetRequiredDelaySinceLastStrike().value());
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(kTestGuid));

  // Add strikes to reach the limit.
  strike_database_->AddStrikes(max_strikes - 1, kTestGuid);
  EXPECT_EQ(strike_database_->GetStrikes(kTestGuid), max_strikes);
  EXPECT_EQ(strike_database_->GetMaxStrikesLimit(), max_strikes);

  // Advance time and verify the feature should be blocked.
  test_autofill_clock.Advance(
      strike_database_->GetRequiredDelaySinceLastStrike().value());
  EXPECT_TRUE(strike_database_->ShouldBlockFeature(kTestGuid));

  // Remove one strike.
  strike_database_->RemoveStrike(kTestGuid);
  EXPECT_EQ(strike_database_->GetStrikes(kTestGuid), max_strikes - 1);

  // Verify the feature should be blocked since it is within the enforced delay
  // period.
  EXPECT_TRUE(strike_database_->ShouldBlockFeature(kTestGuid));

  // Advance time and verify the feature should not be blocked.
  test_autofill_clock.Advance(
      strike_database_->GetRequiredDelaySinceLastStrike().value());
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(kTestGuid));
}

TEST_F(CvcStorageStrikeDatabaseTest, PassExpiryTime) {
  std::string kTestGuid = "00000000-0000-0000-0000-000000000001";
  TestAutofillClock test_autofill_clock(AutofillClock::Now());

  // Add a strike.
  strike_database_->AddStrikes(3, kTestGuid);
  EXPECT_EQ(strike_database_->GetStrikes(kTestGuid), 3);

  // Advance clock to past expiry time and one strike should be removed.
  test_autofill_clock.Advance(strike_database_->GetExpiryTimeDelta().value() +
                              base::Microseconds(1));

  // Make a new CvcStorageStrikeDatabase with the backing test strike database
  // which will remove expired strikes on startup.
  std::unique_ptr<CvcStorageStrikeDatabase> tmp_db =
      std::make_unique<CvcStorageStrikeDatabase>(
          strike_database_service_.get());

  // Verify a strike got removed.
  EXPECT_EQ(tmp_db->GetStrikes(kTestGuid), 2);
}

}  // namespace

}  // namespace autofill
