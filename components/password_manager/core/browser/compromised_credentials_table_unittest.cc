// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/password_manager/core/browser/compromised_credentials_table.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/safe_browsing/core/features.h"
#include "sql/database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

const char kTestDomain[] = "http://example.com/";
const char kTestDomain2[] = "http://test.com/";
const char kTestDomain3[] = "http://google.com/";
const char kUsername[] = "user";
const char kUsername2[] = "user2";
const char kUsername3[] = "user3";

using testing::ElementsAre;
using testing::IsEmpty;

class CompromisedCredentialsTableTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    feature_list_.InitWithFeatures({password_manager::features::kPasswordCheck},
                                   {});
    ReloadDatabase();
  }

  void ReloadDatabase() {
    base::FilePath file = temp_dir_.GetPath().AppendASCII("TestDatabase");
    db_ = std::make_unique<CompromisedCredentialsTable>();
    connection_ = std::make_unique<sql::Database>();
    connection_->set_exclusive_locking();
    ASSERT_TRUE(connection_->Open(file));
    db_->Init(connection_.get());
    ASSERT_TRUE(db_->CreateTableIfNecessary());
  }

  void CheckDatabaseAccessibility() {
    EXPECT_TRUE(db()->AddRow(test_data()));
    EXPECT_THAT(db()->GetAllRows(), ElementsAre(test_data()));
    EXPECT_THAT(db()->GetRows(test_data().signon_realm, test_data().username),
                ElementsAre(test_data()));
    EXPECT_THAT(db()->GetRows(test_data().signon_realm),
                ElementsAre(test_data()));
    EXPECT_TRUE(db()->RemoveRow(test_data().signon_realm, test_data().username,
                                RemoveCompromisedCredentialsReason::kRemove));
    EXPECT_THAT(db()->GetAllRows(), IsEmpty());
    EXPECT_THAT(db()->GetRows(test_data().signon_realm, test_data().username),
                IsEmpty());
    EXPECT_THAT(db()->GetRows(test_data().signon_realm), IsEmpty());
  }

  CompromisedCredentials& test_data() { return test_data_; }
  CompromisedCredentialsTable* db() { return db_.get(); }

  base::test::ScopedFeatureList feature_list_;

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<sql::Database> connection_;
  std::unique_ptr<CompromisedCredentialsTable> db_;
  CompromisedCredentials test_data_{kTestDomain, base::ASCIIToUTF16(kUsername),
                                    base::Time::FromTimeT(1),
                                    CompromiseType::kLeaked};
};

TEST_F(CompromisedCredentialsTableTest, Reload) {
  EXPECT_TRUE(db()->AddRow(test_data()));
  ReloadDatabase();
  EXPECT_THAT(db()->GetAllRows(), ElementsAre(test_data()));
}

TEST_F(CompromisedCredentialsTableTest, DatabaseIsAccessible) {
  // Checks that as long as one experiment is on, the database
  // is accessible. |
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      {safe_browsing::kPasswordProtectionForSignedInUsers},
      {password_manager::features::kPasswordCheck});
  CheckDatabaseAccessibility();

  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      {password_manager::features::kPasswordCheck},
      {safe_browsing::kPasswordProtectionForSignedInUsers});
  CheckDatabaseAccessibility();
}

TEST_F(CompromisedCredentialsTableTest, ExperimentOff) {
  // If the needed experiments are not enabled, the database should not be
  // accessible.
  feature_list_.Reset();
  feature_list_.InitWithFeatures({},
                                 {password_manager::features::kPasswordCheck});
  EXPECT_THAT(db()->GetAllRows(), IsEmpty());
  EXPECT_THAT(db()->GetRows(test_data().signon_realm, test_data().username),
              ElementsAre());
  EXPECT_FALSE(db()->RemoveRow(test_data().signon_realm, test_data().username,
                               RemoveCompromisedCredentialsReason::kRemove));
  EXPECT_TRUE(db()->AddRow(test_data()));
}

TEST_F(CompromisedCredentialsTableTest, SameSignonRealmDifferentUsername) {
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  compromised_credentials2.username = base::ASCIIToUTF16(kUsername2);

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));
  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2));
  EXPECT_THAT(db()->GetRows(test_data().signon_realm),
              ElementsAre(compromised_credentials1, compromised_credentials2));
  EXPECT_THAT(db()->GetRows(test_data().signon_realm, test_data().username),
              ElementsAre(compromised_credentials1));
}

TEST_F(CompromisedCredentialsTableTest, SameUsernameDifferentSignonRealm) {
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  compromised_credentials2.signon_realm = kTestDomain2;

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));
  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2));
  EXPECT_THAT(db()->GetRows(test_data().signon_realm),
              ElementsAre(compromised_credentials1));
  EXPECT_THAT(db()->GetRows(test_data().signon_realm, test_data().username),
              ElementsAre(compromised_credentials1));
}

TEST_F(CompromisedCredentialsTableTest,
       SameSignonRealmAndUsernameDifferentTime) {
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  compromised_credentials2.create_time = base::Time::FromTimeT(2);

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  // It should return false because of unique constraints.
  EXPECT_FALSE(db()->AddRow(compromised_credentials2));
  EXPECT_THAT(db()->GetAllRows(), ElementsAre(compromised_credentials1));
}

TEST_F(CompromisedCredentialsTableTest,
       SameSignonRealmAndUsernameAndDifferentCompromiseType) {
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  compromised_credentials2.compromise_type = CompromiseType::kPhished;

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));
  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2));
  EXPECT_THAT(db()->GetRows(test_data().signon_realm),
              ElementsAre(compromised_credentials1, compromised_credentials2));
  EXPECT_THAT(db()->GetRows(test_data().signon_realm, test_data().username),
              ElementsAre(compromised_credentials1, compromised_credentials2));
}

TEST_F(CompromisedCredentialsTableTest,
       RemovePhishedCredentialByCompromiseType) {
  CompromisedCredentials leaked_credentials = test_data();
  CompromisedCredentials phished_credentials = test_data();
  phished_credentials.compromise_type = CompromiseType::kPhished;

  EXPECT_TRUE(db()->AddRow(leaked_credentials));
  EXPECT_TRUE(db()->AddRow(phished_credentials));
  EXPECT_TRUE(db()->RemoveRowByCompromiseType(
      kTestDomain, base::ASCIIToUTF16(kUsername), CompromiseType::kPhished,
      RemoveCompromisedCredentialsReason::kMarkSiteAsLegitimate));
  EXPECT_THAT(db()->GetAllRows(), ElementsAre(leaked_credentials));
}

TEST_F(CompromisedCredentialsTableTest,
       RemoveLeakedCredentialByCompromiseType) {
  CompromisedCredentials leaked_credentials = test_data();
  CompromisedCredentials phished_credentials = test_data();
  phished_credentials.compromise_type = CompromiseType::kPhished;

  EXPECT_TRUE(db()->AddRow(leaked_credentials));
  EXPECT_TRUE(db()->AddRow(phished_credentials));
  EXPECT_TRUE(db()->RemoveRowByCompromiseType(
      kTestDomain, base::ASCIIToUTF16(kUsername), CompromiseType::kLeaked,
      RemoveCompromisedCredentialsReason::kMarkSiteAsLegitimate));
  EXPECT_THAT(db()->GetAllRows(), ElementsAre(phished_credentials));
}

TEST_F(CompromisedCredentialsTableTest, UpdateRow) {
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2{
      kTestDomain2, base::ASCIIToUTF16(kUsername2), base::Time::FromTimeT(1),
      CompromiseType::kLeaked};

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->UpdateRow(kTestDomain2, base::ASCIIToUTF16(kUsername2),
                              compromised_credentials1.signon_realm,
                              compromised_credentials1.username));
  EXPECT_THAT(db()->GetAllRows(), ElementsAre(compromised_credentials2));
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
  compromised_credentials3.signon_realm = kTestDomain2;
  compromised_credentials4.signon_realm = kTestDomain3;

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));
  EXPECT_TRUE(db()->AddRow(compromised_credentials3));
  EXPECT_TRUE(db()->AddRow(compromised_credentials4));

  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2,
                          compromised_credentials3, compromised_credentials4));

  EXPECT_TRUE(db()->RemoveRowsByUrlAndTime(
      base::BindRepeating(std::not_equal_to<GURL>(),
                          GURL(compromised_credentials1.signon_realm)),
      base::Time(), base::Time::Max()));
  // With unbounded time range and given url filter all rows that are not
  // matching the |compromised_credentials1.signon_realm| should be removed.
  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2));
}

TEST_F(CompromisedCredentialsTableTest, EmptySignonRealm) {
  test_data().signon_realm = "";
  EXPECT_FALSE(db()->AddRow(test_data()));
  EXPECT_THAT(db()->GetAllRows(), IsEmpty());
  EXPECT_FALSE(db()->RemoveRow(test_data().signon_realm, test_data().username,
                               RemoveCompromisedCredentialsReason::kRemove));
}

TEST_F(CompromisedCredentialsTableTest, ReportMetricsBeforeBulkCheck) {
  EXPECT_TRUE(db()->AddRow(test_data()));
  test_data().signon_realm = kTestDomain2;
  test_data().username = base::ASCIIToUTF16(kUsername2);
  EXPECT_TRUE(db()->AddRow(test_data()));
  test_data().signon_realm = kTestDomain3;
  test_data().username = base::ASCIIToUTF16(kUsername3);
  test_data().compromise_type = CompromiseType::kPhished;
  EXPECT_TRUE(db()->AddRow(test_data()));

  base::HistogramTester histogram_tester;
  db()->ReportMetrics(BulkCheckDone(false));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.CompromisedCredentials.CountLeaked", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.CompromisedCredentials.CountPhished", 1, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.CompromisedCredentials.CountLeakedAfterBulkCheck", 0);
}

TEST_F(CompromisedCredentialsTableTest, ReportMetricsAfterBulkCheck) {
  EXPECT_TRUE(db()->AddRow(test_data()));
  test_data().signon_realm = kTestDomain2;
  test_data().username = base::ASCIIToUTF16(kUsername2);
  EXPECT_TRUE(db()->AddRow(test_data()));

  base::HistogramTester histogram_tester;
  db()->ReportMetrics(BulkCheckDone(true));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.CompromisedCredentials.CountLeaked", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.CompromisedCredentials.CountLeakedAfterBulkCheck", 2, 1);
}

}  // namespace
}  // namespace password_manager
