// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/statistics_table.h"

#include <functional>
#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "sql/database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {
namespace {

const char kTestDomain[] = "http://google.com";
const char kTestDomain2[] = "http://example.com";
const char kTestDomain3[] = "https://example.org";
const char kTestDomain4[] = "http://localhost";
const char16_t kUsername1[] = u"user1";
const char16_t kUsername2[] = u"user2";
const char16_t kUsername3[] = u"user3";

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

class StatisticsTableTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ReloadDatabase();

    test_data_.origin_domain = GURL(kTestDomain);
    test_data_.username_value = kUsername1;
    test_data_.dismissal_count = 10;
    test_data_.update_time = base::Time::FromTimeT(1);
  }

  void ReloadDatabase() {
    base::FilePath file = temp_dir_.GetPath().AppendASCII("TestDatabase");
    db_ = std::make_unique<StatisticsTable>();
    connection_ = std::make_unique<sql::Database>(
        sql::DatabaseOptions{.page_size = 4096, .cache_size = 500});
    ASSERT_TRUE(connection_->Open(file));
    db_->Init(connection_.get());
    ASSERT_TRUE(db_->CreateTableIfNecessary());
  }

  InteractionsStats& test_data() { return test_data_; }
  StatisticsTable* db() { return db_.get(); }

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<sql::Database> connection_;
  std::unique_ptr<StatisticsTable> db_;
  InteractionsStats test_data_;
};

TEST_F(StatisticsTableTest, Sanity) {
  EXPECT_TRUE(db()->AddRow(test_data()));
  EXPECT_THAT(db()->GetAllRowsForTest(), ElementsAre(test_data()));
  EXPECT_THAT(db()->GetRows(test_data().origin_domain),
              ElementsAre(test_data()));
  EXPECT_TRUE(db()->RemoveRow(test_data().origin_domain));
  EXPECT_THAT(db()->GetAllRowsForTest(), IsEmpty());
  EXPECT_THAT(db()->GetRows(test_data().origin_domain), IsEmpty());
}

TEST_F(StatisticsTableTest, Reload) {
  EXPECT_TRUE(db()->AddRow(test_data()));

  ReloadDatabase();

  EXPECT_THAT(db()->GetAllRowsForTest(), ElementsAre(test_data()));
  EXPECT_THAT(db()->GetRows(test_data().origin_domain),
              ElementsAre(test_data()));
}

TEST_F(StatisticsTableTest, DoubleOperation) {
  EXPECT_TRUE(db()->AddRow(test_data()));
  test_data().dismissal_count++;
  EXPECT_TRUE(db()->AddRow(test_data()));

  EXPECT_THAT(db()->GetAllRowsForTest(), ElementsAre(test_data()));
  EXPECT_THAT(db()->GetRows(test_data().origin_domain),
              ElementsAre(test_data()));

  EXPECT_TRUE(db()->RemoveRow(test_data().origin_domain));
  EXPECT_THAT(db()->GetAllRowsForTest(), IsEmpty());
  EXPECT_THAT(db()->GetRows(test_data().origin_domain), IsEmpty());
  EXPECT_TRUE(db()->RemoveRow(test_data().origin_domain));
}

TEST_F(StatisticsTableTest, DifferentUsernames) {
  InteractionsStats stats1 = test_data();
  InteractionsStats stats2 = test_data();
  stats2.username_value = kUsername2;

  EXPECT_TRUE(db()->AddRow(stats1));
  EXPECT_TRUE(db()->AddRow(stats2));
  EXPECT_THAT(db()->GetAllRowsForTest(), UnorderedElementsAre(stats1, stats2));
  EXPECT_THAT(db()->GetRows(test_data().origin_domain),
              UnorderedElementsAre(stats1, stats2));
  EXPECT_TRUE(db()->RemoveRow(test_data().origin_domain));
  EXPECT_THAT(db()->GetAllRowsForTest(), IsEmpty());
  EXPECT_THAT(db()->GetRows(test_data().origin_domain), IsEmpty());
}

TEST_F(StatisticsTableTest, RemoveStatsByOriginAndTime) {
  InteractionsStats stats1 = test_data();
  stats1.update_time = base::Time::FromTimeT(1);
  InteractionsStats stats2 = test_data();
  stats2.update_time = base::Time::FromTimeT(2);
  stats2.origin_domain = GURL(kTestDomain2);
  InteractionsStats stats3 = test_data();
  stats3.update_time = base::Time::FromTimeT(2);
  stats3.origin_domain = GURL(kTestDomain3);
  InteractionsStats stats4 = test_data();
  stats4.update_time = base::Time::FromTimeT(2);
  stats4.origin_domain = GURL(kTestDomain4);

  EXPECT_TRUE(db()->AddRow(stats1));
  EXPECT_TRUE(db()->AddRow(stats2));
  EXPECT_TRUE(db()->AddRow(stats3));
  EXPECT_TRUE(db()->AddRow(stats4));
  EXPECT_THAT(db()->GetAllRowsForTest(),
              UnorderedElementsAre(stats1, stats2, stats3, stats4));
  EXPECT_THAT(db()->GetRows(stats1.origin_domain), ElementsAre(stats1));
  EXPECT_THAT(db()->GetRows(stats2.origin_domain), ElementsAre(stats2));
  EXPECT_THAT(db()->GetRows(stats3.origin_domain), ElementsAre(stats3));
  EXPECT_THAT(db()->GetRows(stats4.origin_domain), ElementsAre(stats4));

  // Remove the entry with the timestamp 1 with no origin filter.
  EXPECT_TRUE(db()->RemoveStatsByOriginAndTime(
      base::NullCallback(), base::Time(), base::Time::FromTimeT(2)));
  EXPECT_THAT(db()->GetAllRowsForTest(),
              UnorderedElementsAre(stats2, stats3, stats4));
  EXPECT_THAT(db()->GetRows(stats1.origin_domain), IsEmpty());
  EXPECT_THAT(db()->GetRows(stats2.origin_domain), ElementsAre(stats2));
  EXPECT_THAT(db()->GetRows(stats3.origin_domain), ElementsAre(stats3));
  EXPECT_THAT(db()->GetRows(stats4.origin_domain), ElementsAre(stats4));

  // Remove the entries with the timestamp 2 that are NOT matching
  // |kTestDomain3|.
  EXPECT_TRUE(db()->RemoveStatsByOriginAndTime(
      // Can't use the generic `std::not_equal_to<>` here, because BindRepeating
      // does not support functors with an overloaded call operator.
      // NOLINTNEXTLINE(modernize-use-transparent-functors)
      base::BindRepeating(std::not_equal_to<GURL>(), stats3.origin_domain),
      base::Time::FromTimeT(2), base::Time()));
  EXPECT_THAT(db()->GetAllRowsForTest(), ElementsAre(stats3));
  EXPECT_THAT(db()->GetRows(stats1.origin_domain), IsEmpty());
  EXPECT_THAT(db()->GetRows(stats2.origin_domain), IsEmpty());
  EXPECT_THAT(db()->GetRows(stats3.origin_domain), ElementsAre(stats3));
  EXPECT_THAT(db()->GetRows(stats4.origin_domain), IsEmpty());

  // Remove the entries with the timestamp 2 with no origin filter.
  // This should delete the remaining entry.
  EXPECT_TRUE(db()->RemoveStatsByOriginAndTime(
      base::NullCallback(), base::Time::FromTimeT(2), base::Time()));
  EXPECT_THAT(db()->GetAllRowsForTest(), IsEmpty());
  EXPECT_THAT(db()->GetRows(stats1.origin_domain), IsEmpty());
  EXPECT_THAT(db()->GetRows(stats2.origin_domain), IsEmpty());
  EXPECT_THAT(db()->GetRows(stats3.origin_domain), IsEmpty());
  EXPECT_THAT(db()->GetRows(stats4.origin_domain), IsEmpty());
}

TEST_F(StatisticsTableTest, BadURL) {
  test_data().origin_domain = GURL("trash");
  EXPECT_FALSE(db()->AddRow(test_data()));
  EXPECT_THAT(db()->GetAllRowsForTest(), IsEmpty());
  EXPECT_THAT(db()->GetRows(test_data().origin_domain), IsEmpty());
  EXPECT_FALSE(db()->RemoveRow(test_data().origin_domain));
}

TEST_F(StatisticsTableTest, EmptyURL) {
  test_data().origin_domain = GURL();
  EXPECT_FALSE(db()->AddRow(test_data()));
  EXPECT_THAT(db()->GetAllRowsForTest(), IsEmpty());
  EXPECT_THAT(db()->GetRows(test_data().origin_domain), IsEmpty());
  EXPECT_FALSE(db()->RemoveRow(test_data().origin_domain));
}

TEST_F(StatisticsTableTest, GetDomainsAndAccountsDomainsWithNDismissals) {
  struct {
    const char* origin;
    const char16_t* username;
    int dismissal_count;
  } const stats_database_entries[] = {
      {kTestDomain, kUsername1, 10},   // A
      {kTestDomain, kUsername2, 10},   // B
      {kTestDomain, kUsername3, 1},    // C
      {kTestDomain2, kUsername1, 1},   // D
      {kTestDomain3, kUsername1, 10},  // E
  };
  for (const auto& entry : stats_database_entries) {
    EXPECT_TRUE(db()->AddRow({
        .origin_domain = GURL(entry.origin),
        .username_value = entry.username,
        .dismissal_count = entry.dismissal_count,
        .update_time = base::Time::FromTimeT(1),
    }));
  }

  EXPECT_EQ(5, db()->GetNumAccounts());  // A,B,C,D,E
}

}  // namespace
}  // namespace password_manager
