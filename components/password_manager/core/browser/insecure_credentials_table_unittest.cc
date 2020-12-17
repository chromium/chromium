// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/password_manager/core/browser/insecure_credentials_table.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/safe_browsing/core/features.h"
#include "sql/database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

constexpr char kTestDomain[] = "http://example.com/";
constexpr char kTestDomain2[] = "http://test.com/";
constexpr char kTestDomain3[] = "http://google.com/";
constexpr char kUsername[] = "user";
constexpr char kUsername2[] = "user2";
constexpr char kUsername3[] = "user3";

using testing::ElementsAre;
using testing::IsEmpty;
using testing::SizeIs;

PasswordForm TestForm() {
  PasswordForm form;
  form.signon_realm = kTestDomain;
  form.url = GURL(form.signon_realm);
  form.username_value = base::ASCIIToUTF16(kUsername);
  form.password_value = base::ASCIIToUTF16("1234");
  return form;
}

class InsecureCredentialsTableTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    OSCryptMocker::SetUp();
    ReloadDatabase();
  }

  void TearDown() override {
    login_db_.reset();
    OSCryptMocker::TearDown();
  }

  void ReloadDatabase() {
    base::FilePath file = temp_dir_.GetPath().AppendASCII("TestDatabase");
    login_db_ = std::make_unique<LoginDatabase>(file, IsAccountStore(false));
    ASSERT_TRUE(login_db_->Init());
  }

  CompromisedCredentials& test_data() { return test_data_; }
  PasswordForm& test_form() { return test_form_; }
  InsecureCredentialsTable* db() {
    return &login_db_->insecure_credentials_table();
  }
  LoginDatabase* login_db() { return login_db_.get(); }

 private:
  base::ScopedTempDir temp_dir_;
  // Required for iOS.
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<LoginDatabase> login_db_;
  CompromisedCredentials test_data_{kTestDomain, base::ASCIIToUTF16(kUsername),
                                    base::Time::FromTimeT(1),
                                    CompromiseType::kLeaked, IsMuted(false)};
  PasswordForm test_form_ = TestForm();
};

TEST_F(InsecureCredentialsTableTest, Reload) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  EXPECT_TRUE(db()->AddRow(test_data()));
  ReloadDatabase();
  EXPECT_THAT(db()->GetAllRows(), ElementsAre(test_data()));
}

TEST_F(InsecureCredentialsTableTest, AddWithoutPassword) {
  // The call fails because there is no password stored.
  EXPECT_FALSE(db()->AddRow(test_data()));
  EXPECT_THAT(db()->GetAllRows(), IsEmpty());
}

TEST_F(InsecureCredentialsTableTest, CascadeDelete) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  EXPECT_TRUE(db()->AddRow(test_data()));
  EXPECT_TRUE(login_db()->RemoveLogin(test_form(), nullptr));
  // The compromised entry is also gone silently.
  EXPECT_THAT(db()->GetAllRows(), IsEmpty());
}

TEST_F(InsecureCredentialsTableTest, SameSignonRealmDifferentUsername) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  test_form().username_value = compromised_credentials2.username =
      base::ASCIIToUTF16(kUsername2);
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));
  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2));
  EXPECT_THAT(db()->GetRows(test_data().signon_realm),
              ElementsAre(compromised_credentials1, compromised_credentials2));
}

TEST_F(InsecureCredentialsTableTest, SameUsernameDifferentSignonRealm) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  test_form().signon_realm = compromised_credentials2.signon_realm =
      kTestDomain2;
  test_form().url = GURL(test_form().signon_realm);
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));
  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2));
  EXPECT_THAT(db()->GetRows(test_data().signon_realm),
              ElementsAre(compromised_credentials1));
}

TEST_F(InsecureCredentialsTableTest, SameSignonRealmAndUsernameDifferentTime) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  compromised_credentials2.create_time = base::Time::FromTimeT(2);

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  // It should return false because of unique constraints.
  EXPECT_FALSE(db()->AddRow(compromised_credentials2));
  EXPECT_THAT(db()->GetAllRows(), ElementsAre(compromised_credentials1));
}

TEST_F(InsecureCredentialsTableTest,
       SameSignonRealmAndUsernameAndDifferentCompromiseType) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  compromised_credentials2.compromise_type = CompromiseType::kPhished;
  CompromisedCredentials compromised_credentials3 = test_data();
  compromised_credentials3.compromise_type = CompromiseType::kWeak;

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));
  EXPECT_TRUE(db()->AddRow(compromised_credentials3));
  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2,
                          compromised_credentials3));
  EXPECT_THAT(db()->GetRows(test_data().signon_realm),
              ElementsAre(compromised_credentials1, compromised_credentials2,
                          compromised_credentials3));
}

TEST_F(InsecureCredentialsTableTest, RemoveRow) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  EXPECT_TRUE(db()->AddRow(test_data()));
  EXPECT_THAT(db()->GetRows(test_data().signon_realm),
              ElementsAre(test_data()));

  EXPECT_TRUE(db()->RemoveRow(test_data().signon_realm, test_data().username,
                              RemoveCompromisedCredentialsReason::kUpdate));

  EXPECT_THAT(db()->GetAllRows(), IsEmpty());
  EXPECT_THAT(db()->GetRows(test_data().signon_realm), IsEmpty());
}

TEST_F(InsecureCredentialsTableTest, RemoveRowsCreatedBetween) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  test_form().username_value = base::ASCIIToUTF16(kUsername2);
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  test_form().username_value = base::ASCIIToUTF16(kUsername3);
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
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

TEST_F(InsecureCredentialsTableTest, RemoveRowsCreatedBetweenEdgeCase) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  test_form().username_value = base::ASCIIToUTF16(kUsername2);
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
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

TEST_F(InsecureCredentialsTableTest, RemoveRowsCreatedUpUntilNow) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  test_form().username_value = base::ASCIIToUTF16(kUsername2);
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  test_form().username_value = base::ASCIIToUTF16(kUsername3);
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
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

TEST_F(InsecureCredentialsTableTest, RemoveRowsByUrlAndTime) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  CompromisedCredentials compromised_credentials3 = test_data();
  CompromisedCredentials compromised_credentials4 = test_data();
  test_form().username_value = compromised_credentials2.username =
      base::ASCIIToUTF16(kUsername2);
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  test_form().username_value = compromised_credentials3.username;
  test_form().signon_realm = compromised_credentials3.signon_realm =
      kTestDomain2;
  test_form().url = GURL(test_form().signon_realm);
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  test_form().signon_realm = compromised_credentials4.signon_realm =
      kTestDomain3;
  test_form().url = GURL(test_form().signon_realm);
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));
  EXPECT_TRUE(db()->AddRow(compromised_credentials3));
  EXPECT_TRUE(db()->AddRow(compromised_credentials4));

  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2,
                          compromised_credentials3, compromised_credentials4));

  EXPECT_TRUE(db()->RemoveRowsByUrlAndTime(
      // Can't use the generic `std::not_equal_to<>` here, because BindRepeating
      // does not support functors with an overloaded call operator.
      // NOLINTNEXTLINE(modernize-use-transparent-functors)
      base::BindRepeating(std::not_equal_to<GURL>(),
                          GURL(compromised_credentials1.signon_realm)),
      base::Time(), base::Time::Max()));
  // With unbounded time range and given url filter all rows that are not
  // matching the |compromised_credentials1.signon_realm| should be removed.
  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2));
}

TEST_F(InsecureCredentialsTableTest, ReportMetricsBeforeBulkCheck) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  EXPECT_TRUE(db()->AddRow(test_data()));
  test_form().signon_realm = test_data().signon_realm = kTestDomain2;
  test_form().url = GURL(test_form().signon_realm);
  test_form().username_value = test_data().username =
      base::ASCIIToUTF16(kUsername2);
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  EXPECT_TRUE(db()->AddRow(test_data()));
  test_form().signon_realm = test_data().signon_realm = kTestDomain3;
  test_form().url = GURL(test_form().signon_realm);
  test_form().username_value = test_data().username =
      base::ASCIIToUTF16(kUsername3);
  test_data().compromise_type = CompromiseType::kPhished;
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
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

TEST_F(InsecureCredentialsTableTest, ReportMetricsAfterBulkCheck) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  EXPECT_TRUE(db()->AddRow(test_data()));
  test_form().signon_realm = test_data().signon_realm = kTestDomain2;
  test_form().url = GURL(test_form().signon_realm);
  test_form().username_value = test_data().username =
      base::ASCIIToUTF16(kUsername2);
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
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
