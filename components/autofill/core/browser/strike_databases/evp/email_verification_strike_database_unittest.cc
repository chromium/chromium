// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/evp/email_verification_strike_database.h"

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/strike_database/strike_database.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

namespace autofill {
namespace {

// Verifies that GetId returns a 2-character hex-encoded string.
TEST(EmailVerificationStrikeDatabaseTest, GetIdReturnsTwoCharHex) {
  std::string id = EmailVerificationStrikeDatabase::GetId("test@example.com");
  EXPECT_EQ(id.length(), 2u);
  // Verify it's hex.
  EXPECT_TRUE(absl::ascii_isxdigit(static_cast<unsigned char>(id[0])));
  EXPECT_TRUE(absl::ascii_isxdigit(static_cast<unsigned char>(id[1])));
}

// Verifies that GetId produces consistent outputs for identical inputs.
TEST(EmailVerificationStrikeDatabaseTest, GetIdIsConsistent) {
  EXPECT_EQ(EmailVerificationStrikeDatabase::GetId("test@example.com"),
            EmailVerificationStrikeDatabase::GetId("test@example.com"));
}

// Verifies the traits configured for the strike database.
TEST(EmailVerificationStrikeDatabaseTest, Traits) {
  EXPECT_EQ(EmailVerificationStrikeDatabaseTraits::kName, "EmailVerification");
  EXPECT_EQ(EmailVerificationStrikeDatabaseTraits::kMaxStrikeLimit, 3u);
  EXPECT_EQ(EmailVerificationStrikeDatabaseTraits::kMaxStrikeEntities, 20u);
  EXPECT_EQ(
      EmailVerificationStrikeDatabaseTraits::kMaxStrikeEntitiesAfterCleanup,
      std::nullopt);
  EXPECT_EQ(EmailVerificationStrikeDatabaseTraits::kExpiryTimeDelta,
            base::Days(180));
  EXPECT_TRUE(EmailVerificationStrikeDatabaseTraits::kUniqueIdRequired);
}

class EmailVerificationStrikeDatabaseIntegrationTest : public testing::Test {
 public:
  EmailVerificationStrikeDatabaseIntegrationTest() = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());

    strike_database_service_ =
        std::make_unique<strike_database::StrikeDatabase>(db_provider_.get(),
                                                          temp_dir_.GetPath());

    strike_database_ = std::make_unique<EmailVerificationStrikeDatabase>(
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
  std::unique_ptr<EmailVerificationStrikeDatabase> strike_database_;
};

// Verifies that strikes can be added and the feature is blocked once the strike
// limit is reached.
TEST_F(EmailVerificationStrikeDatabaseIntegrationTest,
       AddStrikesAndShouldBlockFeature) {
  std::string id = EmailVerificationStrikeDatabase::GetId("test@example.com");

  EXPECT_EQ(strike_database_->GetStrikes(id), 0);
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(id));

  // 1 strike
  EXPECT_EQ(strike_database_->AddStrike(id), 1);
  EXPECT_EQ(strike_database_->GetStrikes(id), 1);
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(id));

  // 2 strikes
  EXPECT_EQ(strike_database_->AddStrike(id), 2);
  EXPECT_EQ(strike_database_->GetStrikes(id), 2);
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(id));

  // 3 strikes (reaches max strike limit) -> blocked
  EXPECT_EQ(strike_database_->AddStrike(id), 3);
  EXPECT_EQ(strike_database_->GetStrikes(id), 3);
  EXPECT_TRUE(strike_database_->ShouldBlockFeature(id));
}

// Verifies that clearing strikes for an ID resets the strike count and unblocks
// the feature.
TEST_F(EmailVerificationStrikeDatabaseIntegrationTest, ClearStrikes) {
  std::string id = EmailVerificationStrikeDatabase::GetId("test@example.com");

  strike_database_->AddStrikes(3, id);
  ASSERT_TRUE(strike_database_->ShouldBlockFeature(id));

  strike_database_->ClearStrikes(id);
  EXPECT_EQ(strike_database_->GetStrikes(id), 0);
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(id));
}

// Verifies that ClearAllStrikes clears strikes across all IDs.
TEST_F(EmailVerificationStrikeDatabaseIntegrationTest, ClearAllStrikes) {
  std::string id1 = EmailVerificationStrikeDatabase::GetId("user1@example.com");
  std::string id2 = EmailVerificationStrikeDatabase::GetId("user2@example.com");

  strike_database_->AddStrikes(3, id1);
  strike_database_->AddStrikes(2, id2);
  ASSERT_TRUE(strike_database_->ShouldBlockFeature(id1));
  ASSERT_EQ(strike_database_->GetStrikes(id2), 2);

  strike_database_->ClearAllStrikes();
  EXPECT_EQ(strike_database_->GetStrikes(id1), 0);
  EXPECT_EQ(strike_database_->GetStrikes(id2), 0);
  EXPECT_FALSE(strike_database_->ShouldBlockFeature(id1));
}

// Verifies that strikes expire and the feature becomes unblocked after the
// 180-day expiry window.
TEST_F(EmailVerificationStrikeDatabaseIntegrationTest,
       StrikesExpireAfter180Days) {
  std::string id = EmailVerificationStrikeDatabase::GetId("test@example.com");

  strike_database_->AddStrikes(3, id);
  EXPECT_TRUE(strike_database_->ShouldBlockFeature(id));

  // Fast forward by 179 days: creating a new DB still keeps it blocked.
  task_environment_.FastForwardBy(base::Days(179));
  auto strike_db_day179 = std::make_unique<EmailVerificationStrikeDatabase>(
      strike_database_service_.get());
  EXPECT_TRUE(strike_db_day179->ShouldBlockFeature(id));

  // Fast forward by 2 more days (total 181 days > 180 days):
  // creating a new DB triggers RemoveExpiredStrikes() which reduces strikes
  // below the max limit (3 -> 2), unblocking the feature.
  task_environment_.FastForwardBy(base::Days(2));
  auto strike_db_day181 = std::make_unique<EmailVerificationStrikeDatabase>(
      strike_database_service_.get());
  EXPECT_FALSE(strike_db_day181->ShouldBlockFeature(id));
  EXPECT_EQ(strike_db_day181->GetStrikes(id), 2);
}

}  // namespace
}  // namespace autofill
