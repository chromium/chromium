// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/strike_database_integrator_test_strike_database.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/browser/strike_databases/autofill_profile_update_strike_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class AutofillProfileUpdateStrikeDatabaseTest : public ::testing::Test {
 public:
  AutofillProfileUpdateStrikeDatabaseTest() = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());
    strike_database_service_ = std::make_unique<StrikeDatabase>(
        db_provider_.get(), temp_dir_.GetPath());
    strike_database_ = std::make_unique<AutofillProfileUpdateStrikeDatabase>(
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
  std::unique_ptr<AutofillProfileUpdateStrikeDatabase> strike_database_;
};

TEST_F(AutofillProfileUpdateStrikeDatabaseTest, AddAndRemoveStrikes) {
  std::string test_guid = "a21f010a-eac1-41fc-aee9-c06bbedfb292";
  strike_database_->AddStrike(test_guid);
  EXPECT_EQ(strike_database_->GetStrikes(test_guid), 1);
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(test_guid));

  strike_database_->AddStrikes(2, test_guid);
  EXPECT_EQ(strike_database_->GetStrikes(test_guid), 3);
  EXPECT_TRUE(strike_database_->ShouldBlockFeature(test_guid));

  strike_database_->RemoveStrike(test_guid);
  EXPECT_EQ(strike_database_->GetStrikes(test_guid), 2);
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(test_guid));
}

}  // namespace

}  // namespace autofill
