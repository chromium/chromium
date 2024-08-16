// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/insecure_credentials_table.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/login_database.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "sql/database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

constexpr char kTestDomain[] = "http://example.com/";
constexpr char16_t kUsername[] = u"user";
constexpr char16_t kUsername2[] = u"user2";

using testing::ElementsAre;
using testing::IsEmpty;
using testing::SizeIs;
using testing::UnorderedElementsAre;

PasswordForm TestForm() {
  PasswordForm form;
  form.signon_realm = kTestDomain;
  form.url = GURL(form.signon_realm);
  form.username_value = kUsername;
  form.password_value = u"1234";
  return form;
}

InsecurityMetadata ToInsecurityMetadata(const InsecureCredential& insecure) {
  return InsecurityMetadata(insecure.create_time, insecure.is_muted,
                            insecure.trigger_notification_from_backend);
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
    ASSERT_TRUE(login_db_->Init(base::NullCallback(), nullptr));
  }

  std::vector<int> GetParentIds(
      base::span<const InsecureCredential> credentials) {
    std::vector<int> ids;
    ids.reserve(credentials.size());
    for (const auto& credential : credentials) {
      ids.push_back(credential.parent_key.value());
    }
    return ids;
  }

  InsecureCredential& test_data() { return test_data_; }
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
  InsecureCredential test_data_{
      kTestDomain,           kUsername,      base::Time::FromTimeT(1),
      InsecureType::kLeaked, IsMuted(false), TriggerBackendNotification(true)};
  PasswordForm test_form_ = TestForm();
};

TEST_F(InsecureCredentialsTableTest, Reload) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  EXPECT_TRUE(db()->InsertOrReplace(FormPrimaryKey(1),
                                    test_data().insecure_type,
                                    ToInsecurityMetadata(test_data())));
  ReloadDatabase();
  EXPECT_THAT(db()->GetRows(FormPrimaryKey(1)), ElementsAre(test_data()));
}

TEST_F(InsecureCredentialsTableTest, IsMutedAccountedInCompare) {
  InsecureCredential credential1 = test_data();
  InsecureCredential credential2 = test_data();
  credential2.is_muted = IsMuted(true);
  EXPECT_FALSE(credential1 == credential2);
}

TEST_F(InsecureCredentialsTableTest, CascadeDelete) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  EXPECT_TRUE(db()->InsertOrReplace(FormPrimaryKey(1),
                                    test_data().insecure_type,
                                    ToInsecurityMetadata(test_data())));
  ASSERT_THAT(db()->GetRows(FormPrimaryKey(1)), ElementsAre(test_data()));
  EXPECT_TRUE(login_db()->RemoveLogin(test_form(), nullptr));
  // The compromised entry is also gone silently.
  EXPECT_THAT(db()->GetRows(FormPrimaryKey(1)), IsEmpty());
}

TEST_F(InsecureCredentialsTableTest,
       InsecureCredentialsAddedForDifferentforms) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  InsecureCredential compromised_credentials1 = test_data();
  InsecureCredential compromised_credentials2 = test_data();
  test_form().username_value = compromised_credentials2.username = kUsername2;
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));

  EXPECT_TRUE(db()->InsertOrReplace(
      FormPrimaryKey(1), compromised_credentials1.insecure_type,
      ToInsecurityMetadata(compromised_credentials1)));
  EXPECT_TRUE(db()->InsertOrReplace(
      FormPrimaryKey(2), compromised_credentials2.insecure_type,
      ToInsecurityMetadata(compromised_credentials2)));
  EXPECT_THAT(db()->GetRows(FormPrimaryKey(1)),
              ElementsAre(compromised_credentials1));
  EXPECT_THAT(db()->GetRows(FormPrimaryKey(2)),
              ElementsAre(compromised_credentials2));
}

TEST_F(InsecureCredentialsTableTest, SameSignonRealmAndUsernameDifferentTime) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  InsecureCredential compromised_credentials1 = test_data();
  InsecureCredential compromised_credentials2 = test_data();
  compromised_credentials2.create_time = base::Time::FromTimeT(2);

  EXPECT_TRUE(db()->InsertOrReplace(
      FormPrimaryKey(1), compromised_credentials1.insecure_type,
      ToInsecurityMetadata(compromised_credentials1)));
  EXPECT_THAT(db()->GetRows(FormPrimaryKey(1)),
              ElementsAre(compromised_credentials1));

  EXPECT_TRUE(db()->InsertOrReplace(
      FormPrimaryKey(1), compromised_credentials2.insecure_type,
      ToInsecurityMetadata(compromised_credentials2)));
  // Expect that the original credential has been updated.
  EXPECT_THAT(db()->GetRows(FormPrimaryKey(1)),
              ElementsAre(compromised_credentials2));
}

TEST_F(InsecureCredentialsTableTest,
       AddSameSignonRealmAndUsernameAndDifferentInsecureType) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  InsecureCredential compromised_credentials1 = test_data();
  InsecureCredential compromised_credentials2 = test_data();
  compromised_credentials2.insecure_type = InsecureType::kPhished;
  InsecureCredential compromised_credentials3 = test_data();
  compromised_credentials3.insecure_type = InsecureType::kWeak;

  EXPECT_TRUE(db()->InsertOrReplace(
      FormPrimaryKey(1), compromised_credentials1.insecure_type,
      ToInsecurityMetadata(compromised_credentials1)));
  EXPECT_TRUE(db()->InsertOrReplace(
      FormPrimaryKey(1), compromised_credentials2.insecure_type,
      ToInsecurityMetadata(compromised_credentials2)));
  EXPECT_TRUE(db()->InsertOrReplace(
      FormPrimaryKey(1), compromised_credentials3.insecure_type,
      ToInsecurityMetadata(compromised_credentials3)));
  EXPECT_THAT(
      db()->GetRows(FormPrimaryKey(1)),
      UnorderedElementsAre(compromised_credentials1, compromised_credentials2,
                           compromised_credentials3));
}

TEST_F(InsecureCredentialsTableTest, RemoveRowMultipleTypes) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  InsecureCredential leaked = test_data();
  leaked.insecure_type = InsecureType::kLeaked;
  InsecureCredential phished = test_data();
  phished.insecure_type = InsecureType::kPhished;
  EXPECT_TRUE(db()->InsertOrReplace(FormPrimaryKey(1), leaked.insecure_type,
                                    ToInsecurityMetadata(leaked)));
  EXPECT_TRUE(db()->InsertOrReplace(FormPrimaryKey(1), phished.insecure_type,
                                    ToInsecurityMetadata(phished)));

  EXPECT_THAT(db()->GetRows(FormPrimaryKey(1)),
              UnorderedElementsAre(leaked, phished));

  EXPECT_TRUE(db()->RemoveRow(FormPrimaryKey(1), InsecureType::kPhished));
  EXPECT_THAT(db()->GetRows(FormPrimaryKey(1)), ElementsAre(leaked));
}

TEST_F(InsecureCredentialsTableTest, UpdateRow) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));

  InsecureCredential insecure_credential = test_data();
  insecure_credential.is_muted = IsMuted(false);
  insecure_credential.trigger_notification_from_backend =
      TriggerBackendNotification(true);
  insecure_credential.parent_key = FormPrimaryKey(1);
  EXPECT_TRUE(db()->InsertOrReplace(FormPrimaryKey(1),
                                    insecure_credential.insecure_type,
                                    ToInsecurityMetadata(insecure_credential)));

  EXPECT_THAT(db()->GetRows(FormPrimaryKey(1)),
              UnorderedElementsAre(insecure_credential));

  InsecureCredential new_insecure_credential = insecure_credential;
  new_insecure_credential.is_muted = IsMuted(true);
  new_insecure_credential.trigger_notification_from_backend =
      TriggerBackendNotification(false);
  InsecurityMetadata new_metadata(
      ToInsecurityMetadata(new_insecure_credential));
  EXPECT_TRUE(db()->InsertOrReplace(insecure_credential.parent_key,
                                    insecure_credential.insecure_type,
                                    new_metadata));
  EXPECT_THAT(db()->GetRows(FormPrimaryKey(1)),
              ElementsAre(new_insecure_credential));
}

TEST_F(InsecureCredentialsTableTest, GetAllRowsWithId) {
  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  InsecureCredential compromised_credentials1 = test_data();
  InsecureCredential compromised_credentials2 = test_data();
  compromised_credentials2.insecure_type = InsecureType::kReused;

  EXPECT_TRUE(db()->InsertOrReplace(
      FormPrimaryKey(1), compromised_credentials1.insecure_type,
      ToInsecurityMetadata(compromised_credentials1)));
  EXPECT_TRUE(db()->InsertOrReplace(
      FormPrimaryKey(1), compromised_credentials2.insecure_type,
      ToInsecurityMetadata(compromised_credentials2)));

  EXPECT_THAT(
      db()->GetRows(FormPrimaryKey(1)),
      UnorderedElementsAre(compromised_credentials1, compromised_credentials2));

  test_form().username_value = kUsername2;
  test_data().username = test_form().username_value;

  EXPECT_THAT(login_db()->AddLogin(test_form()), SizeIs(1));
  EXPECT_THAT(db()->GetRows(FormPrimaryKey(2)), IsEmpty());

  compromised_credentials1 = test_data();
  EXPECT_TRUE(db()->InsertOrReplace(
      FormPrimaryKey(2), compromised_credentials1.insecure_type,
      ToInsecurityMetadata(compromised_credentials1)));
  EXPECT_THAT(db()->GetRows(FormPrimaryKey(2)),
              UnorderedElementsAre(compromised_credentials1));
}

}  // namespace
}  // namespace password_manager
