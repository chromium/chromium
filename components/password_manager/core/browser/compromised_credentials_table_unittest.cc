// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/compromised_credentials_table.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "sql/database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

const char kTestDomain[] = "http://example.com";
const char kTestDomain2[] = "http://test.com";
const char kTestDomain3[] = "http://google.com";
const char kUsername[] = "user";
const char kUsername2[] = "user2";
const char kUsername3[] = "user3";

using testing::ElementsAre;
using testing::IsEmpty;

class CompromisedCredentialsTableTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ReloadDatabase();
  }

  void ReloadDatabase() {
    base::FilePath file = temp_dir_.GetPath().AppendASCII("TestDatabase");
    db_.reset(new CompromisedCredentialsTable);
    connection_.reset(new sql::Database);
    connection_->set_exclusive_locking();
    ASSERT_TRUE(connection_->Open(file));
    db_->Init(connection_.get());
    ASSERT_TRUE(db_->CreateTableIfNecessary());
  }

  CompromisedCredentials& test_data() { return test_data_; }
  CompromisedCredentialsTable* db() { return db_.get(); }

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<sql::Database> connection_;
  std::unique_ptr<CompromisedCredentialsTable> db_;
  CompromisedCredentials test_data_{
      GURL(kTestDomain), base::ASCIIToUTF16(kUsername),
      base::Time::FromTimeT(1), CompromiseType::kLeaked};
};

TEST_F(CompromisedCredentialsTableTest, Sanity) {
  EXPECT_TRUE(db()->AddRow(test_data()));
  EXPECT_THAT(db()->GetAllRows(), ElementsAre(test_data()));
  EXPECT_TRUE(db()->RemoveRow(test_data().url, test_data().username));
  EXPECT_THAT(db()->GetAllRows(), IsEmpty());
}

TEST_F(CompromisedCredentialsTableTest, Reload) {
  EXPECT_TRUE(db()->AddRow(test_data()));
  ReloadDatabase();
  EXPECT_THAT(db()->GetAllRows(), ElementsAre(test_data()));
}

TEST_F(CompromisedCredentialsTableTest, SameUrlDifferentUsername) {
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  compromised_credentials2.username = base::ASCIIToUTF16(kUsername2);

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));
  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2));
}

TEST_F(CompromisedCredentialsTableTest, SameUsernameDifferentUrl) {
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  compromised_credentials2.url = GURL(kTestDomain2);

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));
  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2));
}

TEST_F(CompromisedCredentialsTableTest, SameUrlAndUsernameDifferentTime) {
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  compromised_credentials2.create_time = base::Time::FromTimeT(2);

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  // It should return true as the sql statement ran correctly. It ignored
  // new row though because of unique constraints, hence there is only one
  // record in the database.
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));
  EXPECT_THAT(db()->GetAllRows(), ElementsAre(compromised_credentials1));
}

TEST_F(CompromisedCredentialsTableTest,
       SameUrlAndUsernameAndDifferentCompromiseType) {
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  compromised_credentials2.compromise_type = CompromiseType::kPhished;

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));
  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2));
}

TEST_F(CompromisedCredentialsTableTest,
       SameUsernameAndUrlAndDifferentCompromiseType) {
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  compromised_credentials2.compromise_type = CompromiseType::kPhished;

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));
  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2));
}

TEST_F(CompromisedCredentialsTableTest, RemoveRowsCreatedBetween) {
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  CompromisedCredentials compromised_credentials3 = test_data();
  compromised_credentials2.username = base::ASCIIToUTF16(kUsername2);
  compromised_credentials3.username = base::ASCIIToUTF16(kUsername3);
  compromised_credentials1.create_time = base::Time::FromTimeT(10);
  compromised_credentials2.create_time = base::Time::FromTimeT(20);
  compromised_credentials3.create_time = base::Time::FromTimeT(30);

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));
  EXPECT_TRUE(db()->AddRow(compromised_credentials3));

  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2,
                          compromised_credentials3));

  EXPECT_TRUE(db()->RemoveRowsByUrlAndTime(base::NullCallback(),
                                           base::Time::FromTimeT(15),
                                           base::Time::FromTimeT(25)));

  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials3));
}

TEST_F(CompromisedCredentialsTableTest, RemoveRowsCreatedBetweenEdgeCase) {
  base::Time begin_time = base::Time::FromTimeT(10);
  base::Time end_time = base::Time::FromTimeT(20);
  CompromisedCredentials compromised_credentials_begin = test_data();
  CompromisedCredentials compromised_credentials_end = test_data();
  compromised_credentials_begin.create_time = begin_time;
  compromised_credentials_end.create_time = end_time;
  compromised_credentials_end.username = base::ASCIIToUTF16(kUsername2);

  EXPECT_TRUE(db()->AddRow(compromised_credentials_begin));
  EXPECT_TRUE(db()->AddRow(compromised_credentials_end));

  EXPECT_THAT(db()->GetAllRows(), ElementsAre(compromised_credentials_begin,
                                              compromised_credentials_end));

  EXPECT_TRUE(
      db()->RemoveRowsByUrlAndTime(base::NullCallback(), begin_time, end_time));
  // RemoveRowsCreatedBetween takes |begin_time| inclusive and |end_time|
  // exclusive, hence the credentials with |end_time| should remain in the
  // database.
  EXPECT_THAT(db()->GetAllRows(), ElementsAre(compromised_credentials_end));
}

TEST_F(CompromisedCredentialsTableTest, RemoveRowsCreatedUpUntilNow) {
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  CompromisedCredentials compromised_credentials3 = test_data();
  compromised_credentials2.username = base::ASCIIToUTF16(kUsername2);
  compromised_credentials3.username = base::ASCIIToUTF16(kUsername3);
  compromised_credentials1.create_time = base::Time::FromTimeT(42);
  compromised_credentials2.create_time = base::Time::FromTimeT(780);
  compromised_credentials3.create_time = base::Time::FromTimeT(3000);

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));
  EXPECT_TRUE(db()->AddRow(compromised_credentials3));

  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2,
                          compromised_credentials3));

  EXPECT_TRUE(db()->RemoveRowsByUrlAndTime(base::NullCallback(), base::Time(),
                                           base::Time::Max()));

  EXPECT_THAT(db()->GetAllRows(), IsEmpty());
}

TEST_F(CompromisedCredentialsTableTest, RemoveRowsByUrlAndTime) {
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  CompromisedCredentials compromised_credentials3 = test_data();
  CompromisedCredentials compromised_credentials4 = test_data();
  compromised_credentials2.username = base::ASCIIToUTF16(kUsername2);
  compromised_credentials3.url = GURL(kTestDomain2);
  compromised_credentials4.url = GURL(kTestDomain3);

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));
  EXPECT_TRUE(db()->AddRow(compromised_credentials3));
  EXPECT_TRUE(db()->AddRow(compromised_credentials4));

  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2,
                          compromised_credentials3, compromised_credentials4));

  EXPECT_TRUE(db()->RemoveRowsByUrlAndTime(
      base::BindRepeating(std::not_equal_to<GURL>(),
                          compromised_credentials1.url),
      base::Time(), base::Time::Max()));
  // With unbounded time range and given url filter all rows that are not
  // matching the |compromised_credentials1.url| should be removed.
  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2));
}

TEST_F(CompromisedCredentialsTableTest, BadURL) {
  test_data().url = GURL("bad");
  EXPECT_FALSE(db()->AddRow(test_data()));
  EXPECT_THAT(db()->GetAllRows(), IsEmpty());
  EXPECT_FALSE(db()->RemoveRow(test_data().url, test_data().username));
}

TEST_F(CompromisedCredentialsTableTest, EmptyURL) {
  test_data().url = GURL();
  EXPECT_FALSE(db()->AddRow(test_data()));
  EXPECT_THAT(db()->GetAllRows(), IsEmpty());
  EXPECT_FALSE(db()->RemoveRow(test_data().url, test_data().username));
}

}  // namespace
}  // namespace password_manager
