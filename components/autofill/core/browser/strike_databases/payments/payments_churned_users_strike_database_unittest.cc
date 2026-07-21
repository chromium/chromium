// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/payments/payments_churned_users_strike_database.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/strike_database/strike_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

class PaymentsChurnedUsersStrikeDatabaseTest : public ::testing::Test {
 public:
  PaymentsChurnedUsersStrikeDatabaseTest() = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());

    strike_database_service_ =
        std::make_unique<strike_database::StrikeDatabase>(db_provider_.get(),
                                                          temp_dir_.GetPath());

    strike_database_ = std::make_unique<PaymentsChurnedUsersStrikeDatabase>(
        strike_database_service_.get());
  }

  void TearDown() override {
    // The destruction of `strike_database_service_`'s components is posted
    // to a task runner.
    strike_database_.reset();
    strike_database_service_.reset();
    db_provider_.reset();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> db_provider_;
  std::unique_ptr<strike_database::StrikeDatabase> strike_database_service_;
  std::unique_ptr<PaymentsChurnedUsersStrikeDatabase> strike_database_;
};

TEST_F(PaymentsChurnedUsersStrikeDatabaseTest, StrikeDatabaseProperties) {
  EXPECT_EQ(strike_database_->GetMaxStrikesLimit(), 2);
  EXPECT_EQ(strike_database_->GetRequiredDelaySinceLastStrike(),
            base::Days(kPaymentsChurnedUsersEnforcedDelayInDays));
}

}  // namespace

}  // namespace autofill
