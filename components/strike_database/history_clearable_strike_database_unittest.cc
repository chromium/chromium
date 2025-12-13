// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/strike_database/history_clearable_strike_database.h"

#include <optional>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/strike_database/strike_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace strike_database {

namespace {
struct TestStrikeDatabaseTraits {
  static constexpr std::string_view kName = "Test";
  static constexpr size_t kMaxStrikeEntities = 200;
  static constexpr size_t kMaxStrikeEntitiesAfterCleanup = 150;
  static constexpr size_t kMaxStrikeLimit = 3;
  static constexpr base::TimeDelta kExpiryTimeDelta = base::Days(180);
  static constexpr bool kUniqueIdRequired = true;

  static std::string HostFromId(const std::string& id) {
    // To keep testing simple, we assume the database is only keyed by host.
    return id;
  }
};

class HistoryClearableStrikeDatabaseTest : public ::testing::Test {
 public:
  HistoryClearableStrikeDatabaseTest() = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_provider_ = std::make_unique<leveldb_proto::ProtoDatabaseProvider>(
        temp_dir_.GetPath());
    strike_database_service_ = std::make_unique<StrikeDatabase>(
        db_provider_.get(), temp_dir_.GetPath());
    strike_database_ = std::make_unique<
        HistoryClearableStrikeDatabase<TestStrikeDatabaseTraits>>(
        strike_database_service_.get());
    delete_first_url_row_.emplace_back(kUrl1);
    delete_all_urls_rows_.emplace_back(kUrl1);
    delete_all_urls_rows_.emplace_back(kUrl2);
    delete_all_urls_rows_.emplace_back(kUrl3);
  }

  void TearDown() override {
    // The destruction of `strike_database_service_`'s components is posted
    // to a task runner, requires running the loop to complete.
    strike_database_.reset();
    strike_database_service_.reset();
    db_provider_.reset();
    task_environment_.RunUntilIdle();
  }

  void ClearStrikesByOrigin(const history::URLRows& deleted_rows) {
    strike_database_->ClearStrikesWithHistory(
        history::DeletionInfo::ForUrls(deleted_rows,
                                       /*favicon_urls=*/{}));
  }

  void ClearStrikesByOriginAndTime(const history::URLRows& deleted_rows,
                                   base::Time delete_begin,
                                   base::Time delete_end) {
    strike_database_->ClearStrikesWithHistory(history::DeletionInfo(
        history::DeletionTimeRange(delete_begin, delete_end),
        /*is_from_expiration=*/false, deleted_rows,
        /*favicon_urls=*/{}, /*restrict_urls=*/std::nullopt));
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<leveldb_proto::ProtoDatabaseProvider> db_provider_;
  std::unique_ptr<StrikeDatabase> strike_database_service_;
  std::unique_ptr<HistoryClearableStrikeDatabase<TestStrikeDatabaseTraits>>
      strike_database_;

  const GURL kUrl1 = GURL("https://www.strikedhost.com");
  const GURL kUrl2 = GURL("https://www.otherhost.com");
  const GURL kUrl3 = GURL("https://www.justanotherhost.com");

  history::URLRows delete_first_url_row_;
  history::URLRows delete_all_urls_rows_;
};

TEST_F(HistoryClearableStrikeDatabaseTest,
       RemoveStrikesByOriginWithinDeletionWindow) {
  base::Time start_time = base::Time::Now();
  // Both strikes are added within the deletion window, but the second should
  // be ruled out by the filter.
  strike_database_->AddStrike(kUrl1.GetHost());
  strike_database_->AddStrike(kUrl2.GetHost());
  base::Time end_time = base::Time::Now();

  EXPECT_EQ(strike_database_->GetStrikes(kUrl1.GetHost()), 1);
  EXPECT_EQ(strike_database_->GetStrikes(kUrl2.GetHost()), 1);

  ClearStrikesByOriginAndTime(delete_first_url_row_, start_time, end_time);

  EXPECT_EQ(strike_database_->GetStrikes(kUrl1.GetHost()), 0);
  EXPECT_EQ(strike_database_->GetStrikes(kUrl2.GetHost()), 1);
}

TEST_F(HistoryClearableStrikeDatabaseTest, RemoveStrikesByOrigin) {
  strike_database_->AddStrike(kUrl1.GetHost());
  strike_database_->AddStrike(kUrl2.GetHost());

  EXPECT_EQ(strike_database_->GetStrikes(kUrl1.GetHost()), 1);
  EXPECT_EQ(strike_database_->GetStrikes(kUrl2.GetHost()), 1);

  ClearStrikesByOrigin(delete_first_url_row_);

  EXPECT_EQ(strike_database_->GetStrikes(kUrl1.GetHost()), 0);
  EXPECT_EQ(strike_database_->GetStrikes(kUrl2.GetHost()), 1);
}

TEST_F(HistoryClearableStrikeDatabaseTest,
       DoNotRemoveStrikeAfterDeletionWindow) {
  base::Time start_time = base::Time::Now();
  strike_database_->AddStrike(kUrl1.GetHost());
  task_environment_.FastForwardBy(base::Minutes(1));
  base::Time end_time = base::Time::Now();
  task_environment_.FastForwardBy(base::Minutes(1));

  // Now update the time stamp of this entry by adding another strike.
  // By this, the entry should not be deleted.
  strike_database_->AddStrike(kUrl1.GetHost());

  ClearStrikesByOriginAndTime(delete_all_urls_rows_, start_time, end_time);
  EXPECT_EQ(strike_database_->GetStrikes(kUrl1.GetHost()), 2);
}

TEST_F(HistoryClearableStrikeDatabaseTest,
       DoNotRemoveStrikeBeforeDeletionWindow) {
  // The strike is added before the deletion window.
  strike_database_->AddStrike(kUrl1.GetHost());
  task_environment_.FastForwardBy(base::Minutes(1));

  base::Time start_time = base::Time::Now();
  task_environment_.FastForwardBy(base::Minutes(1));
  base::Time end_time = base::Time::Now();

  ClearStrikesByOriginAndTime(delete_all_urls_rows_, start_time, end_time);

  EXPECT_EQ(strike_database_->GetStrikes(kUrl1.GetHost()), 1);
}

}  // namespace

}  // namespace strike_database
