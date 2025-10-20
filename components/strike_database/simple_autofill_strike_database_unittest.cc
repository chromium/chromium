// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/to_string.h"
#include "base/test/task_environment.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/strike_database/simple_strike_database.h"
#include "components/strike_database/strike_data.pb.h"
#include "components/strike_database/strike_database_integrator_test_strike_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace strike_database {

namespace {

struct TestStrikeDatabaseTraits {
  static constexpr std::string_view kName = "TestStrikeDatabase";
  static constexpr size_t kMaxStrikeEntities = 100;
  static constexpr size_t kMaxStrikeEntitiesAfterCleanup = 50;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr std::optional<base::TimeDelta> kExpiryTimeDelta =
      std::nullopt;
  static constexpr bool kUniqueIdRequired = true;
};

using TestStrikeDatabase = SimpleStrikeDatabase<TestStrikeDatabaseTraits>;

class SimpleStrikeDatabaseTest : public ::testing::Test {
 public:
  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());
    strike_database_service_ = std::make_unique<StrikeDatabase>(
        db_provider_.get(), temp_dir_.GetPath());
    strike_database_ =
        std::make_unique<TestStrikeDatabase>(strike_database_service_.get());
  }

  void TearDown() override {
    // The destruction of `strike_database_service_`'s components is posted
    // to a task runner. Wait for the task's completion.
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
  std::unique_ptr<TestStrikeDatabase> strike_database_;
};

// Tests that strikes can be added and removed, and that depending on the
// `kMaxStrikeLimit`, the feature is considered blocked.
TEST_F(SimpleStrikeDatabaseTest, AddAndRemoveStrikes) {
  const std::string test_key = "123";
  strike_database_->AddStrike(test_key);
  EXPECT_EQ(strike_database_->GetStrikes(test_key), 1);
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(test_key));

  strike_database_->AddStrikes(TestStrikeDatabaseTraits::kMaxStrikeLimit - 1,
                               test_key);
  EXPECT_EQ(strike_database_->GetStrikes(test_key),
            static_cast<int>(TestStrikeDatabaseTraits::kMaxStrikeLimit));
  EXPECT_TRUE(strike_database_->ShouldBlockFeature(test_key));

  strike_database_->RemoveStrike(test_key);
  EXPECT_EQ(strike_database_->GetStrikes(test_key),
            static_cast<int>(TestStrikeDatabaseTraits::kMaxStrikeLimit - 1));
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(test_key));
}

// Tests that when too many strikes are added, the oldest strikes are cleared.
TEST_F(SimpleStrikeDatabaseTest, MaxEntries) {
  for (size_t i = 0; i < TestStrikeDatabaseTraits::kMaxStrikeEntities; i++) {
    strike_database_->AddStrike(base::ToString(i));
  }
  EXPECT_EQ(strike_database_->GetStrikes("0"), 1);
  strike_database_->AddStrike(
      base::ToString(TestStrikeDatabaseTraits::kMaxStrikeEntities));
  EXPECT_EQ(strike_database_->GetStrikes("0"), 0);
}

}  // namespace

}  // namespace strike_database
