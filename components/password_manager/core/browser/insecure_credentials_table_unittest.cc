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
using testing::UnorderedElementsAre;

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

  std::vector<int> GetParentIds(
      base::span<const CompromisedCredentials> credentials) {
    std::vector<int> ids;
    ids.reserve(credentials.size());
    for (const auto& credential : credentials)
      ids.push_back(credential.parent_key.value());
    return ids;
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
                                    InsecureType::kLeaked, IsMuted(false)};
  PasswordForm test_form_ = TestForm();
};

TEST_F(InsecureCredentialsTableTest, Reload) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  EXPECT_TRUE(db()->AddRow(test_data()));
  ReloadDatabase();
  EXPECT_THAT(db()->GetAllRows(), ElementsAre(test_data()));
}

TEST_F(InsecureCredentialsTableTest, IsMutedAccountedInCompare) {
  CompromisedCredentials credential1 = test_data();
  CompromisedCredentials credential2 = test_data();
  credential2.is_muted = IsMuted(true);
  EXPECT_FALSE(credential1 == credential2);
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
  EXPECT_THAT(GetParentIds(db()->GetAllRows()), ElementsAre(1, 2));
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
  EXPECT_THAT(GetParentIds(db()->GetAllRows()), ElementsAre(1, 2));
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
  EXPECT_THAT(GetParentIds(db()->GetAllRows()), ElementsAre(1));
}

TEST_F(InsecureCredentialsTableTest,
       SameSignonRealmAndUsernameAndDifferentInsecureType) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  compromised_credentials2.insecure_type = InsecureType::kPhished;
  CompromisedCredentials compromised_credentials3 = test_data();
  compromised_credentials3.insecure_type = InsecureType::kWeak;

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));
  EXPECT_TRUE(db()->AddRow(compromised_credentials3));
  EXPECT_THAT(db()->GetAllRows(),
              ElementsAre(compromised_credentials1, compromised_credentials2,
                          compromised_credentials3));
  EXPECT_THAT(db()->GetRows(test_data().signon_realm),
              ElementsAre(compromised_credentials1, compromised_credentials2,
                          compromised_credentials3));
  EXPECT_THAT(GetParentIds(db()->GetAllRows()), ElementsAre(1, 1, 1));
}

TEST_F(InsecureCredentialsTableTest, RemoveRow) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  EXPECT_TRUE(db()->AddRow(test_data()));
  EXPECT_THAT(db()->GetRows(test_data().signon_realm),
              ElementsAre(test_data()));
  EXPECT_THAT(GetParentIds(db()->GetAllRows()), ElementsAre(1));

  EXPECT_TRUE(db()->RemoveRow(test_data().signon_realm, test_data().username,
                              RemoveInsecureCredentialsReason::kUpdate));

  EXPECT_THAT(db()->GetAllRows(), IsEmpty());
  EXPECT_THAT(db()->GetRows(test_data().signon_realm), IsEmpty());
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
  test_data().insecure_type = InsecureType::kPhished;
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

TEST_F(InsecureCredentialsTableTest, GetAllRowsWithId) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  CompromisedCredentials compromised_credentials1 = test_data();
  CompromisedCredentials compromised_credentials2 = test_data();
  compromised_credentials2.insecure_type = InsecureType::kReused;

  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_TRUE(db()->AddRow(compromised_credentials2));

  EXPECT_THAT(
      db()->GetRows(FormPrimaryKey(1)),
      UnorderedElementsAre(compromised_credentials1, compromised_credentials2));
  EXPECT_THAT(GetParentIds(db()->GetAllRows()), ElementsAre(1, 1));

  test_form().username_value = base::ASCIIToUTF16(kUsername2);
  test_data().username = test_form().username_value;

  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  EXPECT_THAT(db()->GetRows(FormPrimaryKey(2)), IsEmpty());

  compromised_credentials1 = test_data();
  EXPECT_TRUE(db()->AddRow(compromised_credentials1));
  EXPECT_THAT(db()->GetRows(FormPrimaryKey(2)),
              UnorderedElementsAre(compromised_credentials1));
  EXPECT_THAT(GetParentIds(db()->GetAllRows()), ElementsAre(1, 1, 2));
}

}  // namespace
}  // namespace password_manager
