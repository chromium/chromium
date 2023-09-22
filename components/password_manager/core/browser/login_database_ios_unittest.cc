// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/login_database.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <stddef.h>

#include <tuple>

#include "base/files/scoped_temp_dir.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/passwords_directory_util_ios.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::Bucket;
using base::ScopedCFTypeRef;
using base::UTF16ToUTF8;
using password_manager::metrics_util::MigrationToOSCrypt;

namespace {
void ExpectSuccessMetricsRecorded(
    const base::HistogramTester& histogram_tester,
    password_manager::IsAccountStore is_account_store) {
  base::StringPiece store_infix =
      is_account_store ? "AccountStore" : "ProfileStore";

  EXPECT_THAT(
      histogram_tester.GetAllSamples("PasswordManager.MigrationToOSCrypt"),
      BucketsInclude(Bucket(MigrationToOSCrypt::kStarted, 1),
                     Bucket(MigrationToOSCrypt::kSuccess, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(base::StrCat(
                  {"PasswordManager.MigrationToOSCrypt.", store_infix})),
              BucketsInclude(Bucket(MigrationToOSCrypt::kStarted, 1),
                             Bucket(MigrationToOSCrypt::kSuccess, 1)));
  histogram_tester.ExpectTotalCount(
      base::StrCat({"PasswordManager.MigrationToOSCrypt.", store_infix,
                    ".SuccessLatency"}),
      1);
}
}  // namespace
namespace password_manager {

class LoginDatabaseIOSTest : public PlatformTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath login_db_path =
        temp_dir_.GetPath().AppendASCII("temp_login.db");
    login_db_.reset(new password_manager::LoginDatabase(
        login_db_path, password_manager::IsAccountStore(false)));
    login_db_->Init();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<LoginDatabase> login_db_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(LoginDatabaseIOSTest, KeychainStorage) {
  std::u16string test_passwords[] = {
      u"foo",
      u"bar",
      u"\u043F\u0430\u0440\u043E\u043B\u044C",
      std::u16string(),
  };

  for (unsigned int i = 0; i < std::size(test_passwords); i++) {
    std::string encrypted;
    EXPECT_EQ(LoginDatabase::ENCRYPTION_RESULT_SUCCESS,
              login_db_->EncryptedString(test_passwords[i], &encrypted));
    std::u16string decrypted;
    EXPECT_EQ(LoginDatabase::ENCRYPTION_RESULT_SUCCESS,
              login_db_->DecryptedString(encrypted, &decrypted));
    EXPECT_STREQ(UTF16ToUTF8(test_passwords[i]).c_str(),
                 UTF16ToUTF8(decrypted).c_str());
  }
}

TEST_F(LoginDatabaseIOSTest, AddLogin) {
  PasswordForm form;
  form.url = GURL("http://0.com");
  form.signon_realm = "http://www.example.com/";
  form.action = GURL("http://www.example.com/action");
  form.password_element = u"pwd";
  form.password_value = u"example";

  password_manager::PasswordStoreChangeList changes = login_db_->AddLogin(form);
  std::string keychain_identifier = changes[0].form().keychain_identifier;
  ASSERT_FALSE(keychain_identifier.empty());

  std::u16string password_value;
  EXPECT_EQ(errSecSuccess, GetTextFromKeychainIdentifier(keychain_identifier,
                                                         &password_value));
  EXPECT_EQ(form.password_value, password_value);

  // Clear item from the keychain to ensure this test doesn't affect other
  // tests.
  DeleteEncryptedPasswordFromKeychain(keychain_identifier);
}

TEST_F(LoginDatabaseIOSTest, UpdateLogin) {
  PasswordForm form;
  form.url = GURL("http://0.com");
  form.signon_realm = "http://www.example.com";
  form.action = GURL("http://www.example.com/action");
  form.password_element = u"pwd";
  form.password_value = u"example";

  password_manager::PasswordStoreChangeList changes = login_db_->AddLogin(form);
  std::string old_keychain_identifier = changes[0].form().keychain_identifier;

  form.password_value = u"secret";

  ASSERT_THAT(login_db_->UpdateLogin(form), testing::SizeIs(1));

  std::vector<std::unique_ptr<PasswordForm>> forms;
  EXPECT_TRUE(login_db_->GetLogins(PasswordFormDigest(form), true, &forms));

  ASSERT_EQ(1U, forms.size());
  std::string keychain_identifier = forms[0]->keychain_identifier;
  ASSERT_FALSE(keychain_identifier.empty());

  std::u16string password_value;
  EXPECT_EQ(errSecSuccess, GetTextFromKeychainIdentifier(keychain_identifier,
                                                         &password_value));
  EXPECT_EQ(form.password_value, password_value);
  // Check that keychain item corresponding to the old password value is
  // deleted.
  EXPECT_EQ(errSecItemNotFound, GetTextFromKeychainIdentifier(
                                    old_keychain_identifier, &password_value));

  // Clear item from the keychain to ensure this test doesn't affect other
  // tests.
  DeleteEncryptedPasswordFromKeychain(keychain_identifier);
}

TEST_F(LoginDatabaseIOSTest, RemoveLogin) {
  PasswordForm form;
  form.signon_realm = "http://www.example.com";
  form.url = GURL("http://www.example.com/action");
  form.password_element = u"pwd";
  form.password_value = u"example";

  password_manager::PasswordStoreChangeList changes = login_db_->AddLogin(form);
  std::string keychain_identifier = changes[0].form().keychain_identifier;
  std::ignore = login_db_->RemoveLogin(form, /*changes=*/nullptr);

  // Verify that password is no longer available in the keychain.
  std::u16string password_value;
  EXPECT_EQ(errSecItemNotFound, GetTextFromKeychainIdentifier(
                                    keychain_identifier, &password_value));
}

TEST_F(LoginDatabaseIOSTest, RemoveLoginsCreatedBetween) {
  PasswordForm forms[3];
  forms[0].url = GURL("http://0.com");
  forms[0].signon_realm = "http://www.example.com";
  forms[0].username_element = u"login0";
  forms[0].date_created = base::Time::FromDoubleT(100);
  forms[0].password_value = u"pass0";
  forms[0].in_store = PasswordForm::Store::kProfileStore;

  forms[1].url = GURL("http://1.com");
  forms[1].signon_realm = "http://www.example.com";
  forms[1].username_element = u"login1";
  forms[1].date_created = base::Time::FromDoubleT(200);
  forms[1].password_value = u"pass1";
  forms[1].in_store = PasswordForm::Store::kProfileStore;

  forms[2].url = GURL("http://2.com");
  forms[2].signon_realm = "http://www.example.com";
  forms[2].username_element = u"login2";
  forms[2].date_created = base::Time::FromDoubleT(300);
  forms[2].password_value = u"pass2";
  forms[2].in_store = PasswordForm::Store::kProfileStore;

  for (size_t i = 0; i < std::size(forms); i++) {
    std::ignore = login_db_->AddLogin(forms[i]);
  }

  PasswordFormDigest form = {PasswordForm::Scheme::kHtml,
                             "http://www.example.com", GURL()};
  std::vector<std::unique_ptr<PasswordForm>> logins;
  EXPECT_TRUE(login_db_->GetLogins(form, true, &logins));
  ASSERT_EQ(3U, logins.size());
  // Verify that for each password exist a keychain item with a password.
  for (const auto& login : logins) {
    std::u16string password_value;
    EXPECT_EQ(errSecSuccess, GetTextFromKeychainIdentifier(
                                 login->keychain_identifier, &password_value));
    EXPECT_EQ(login->password_value, password_value);
  }

  login_db_->RemoveLoginsCreatedBetween(base::Time::FromDoubleT(150),
                                        base::Time::FromDoubleT(250),
                                        /*changes=*/nullptr);

  // Verify that one password is removed.
  std::vector<std::unique_ptr<PasswordForm>> remaining_logins;
  EXPECT_TRUE(login_db_->GetLogins(form, true, &remaining_logins));
  EXPECT_THAT(remaining_logins, testing::UnorderedElementsAre(
                                    testing::Pointee(testing::Eq(forms[0])),
                                    testing::Pointee(testing::Eq(forms[2]))));

  // Verify that keychain entry is removed.
  std::u16string password_value;
  EXPECT_EQ(errSecSuccess,
            GetTextFromKeychainIdentifier(logins[0]->keychain_identifier,
                                          &password_value));
  EXPECT_EQ(errSecItemNotFound,
            GetTextFromKeychainIdentifier(logins[1]->keychain_identifier,
                                          &password_value));
  EXPECT_EQ(errSecSuccess,
            GetTextFromKeychainIdentifier(logins[2]->keychain_identifier,
                                          &password_value));

  // Clear item from the keychain to ensure this test doesn't affect other
  // tests.
  DeleteEncryptedPasswordFromKeychain(logins[0]->keychain_identifier);
  DeleteEncryptedPasswordFromKeychain(logins[2]->keychain_identifier);
}

class LoginDatabaseMigrationToOSCryptTest
    : public LoginDatabaseIOSTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    database_path_ = temp_dir_.GetPath().AppendASCII("test.db");
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kOneReadLoginDatabaseMigration},
          /*disabled_features=*/{});
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kOneReadLoginDatabaseMigration});
    }
  }

  // Creates the database from |sql_file|.
  void CreateDatabase(base::StringPiece sql_file) {
    base::FilePath database_dump;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &database_dump));
    database_dump = database_dump.AppendASCII("components")
                        .AppendASCII("test")
                        .AppendASCII("data")
                        .AppendASCII("password_manager")
                        .AppendASCII(sql_file);
    ASSERT_TRUE(
        sql::test::CreateDatabaseFromSQL(database_path_, database_dump));
  }

  void AddItemToKeychain(const std::u16string& value, const std::string& guid) {
    ScopedCFTypeRef<CFStringRef> item_ref(base::SysUTF8ToCFStringRef(guid));
    ScopedCFTypeRef<CFMutableDictionaryRef> attributes(
        CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks));
    CFDictionarySetValue(attributes, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(attributes, kSecAttrAccount, item_ref);
    std::string plain_text_utf8 = base::UTF16ToUTF8(value);
    ScopedCFTypeRef<CFDataRef> data(CFDataCreate(
        NULL, reinterpret_cast<const UInt8*>(plain_text_utf8.data()),
        plain_text_utf8.size()));
    CFDictionarySetValue(attributes, kSecValueData, data);
    EXPECT_EQ(errSecSuccess, SecItemAdd(attributes, NULL));
  }

  std::vector<std::string> GetEncryptedPasswordValues() const {
    sql::Database db;
    CHECK(db.Open(database_path_));

    sql::Statement s(db.GetCachedStatement(
        SQL_FROM_HERE, "SELECT password_value FROM logins"));
    EXPECT_TRUE(s.is_valid());

    std::vector<std::string> results;
    while (s.Step()) {
      std::string encrypted_password;
      s.ColumnBlobAsString(0, &encrypted_password);
      results.push_back(std::move(encrypted_password));
    }

    return results;
  }

  void ReplacePasswordValue(const std::string& new_value) {
    sql::Database db;
    CHECK(db.Open(get_database_path()));
    sql::Statement new_password_value(db.GetCachedStatement(
        SQL_FROM_HERE, "UPDATE logins SET password_value = ?"));
    new_password_value.BindString(0, new_value);
    ASSERT_TRUE(new_password_value.Run());
  }

  void ReplaceNoteValue(const std::string& new_value) {
    sql::Database db;
    CHECK(db.Open(get_database_path()));
    sql::Statement new_not_value(db.GetCachedStatement(
        SQL_FROM_HERE, "UPDATE password_notes SET value = ?"));
    new_not_value.BindString(0, new_value);
    ASSERT_TRUE(new_not_value.Run());
  }

  base::FilePath get_database_path() { return database_path_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir temp_dir_;
  base::FilePath database_path_;
};

// Tests the migration of the login database from version() to
// kCurrentVersionNumber.
TEST_P(LoginDatabaseMigrationToOSCryptTest, MigrationToVersion39ProfileStore) {
  // Keychain identifier used in the test file.
  const std::string password_keychain_identifier =
      "2572a7dc-5046-429b-b8d4-3696f87dc9c2";
  const std::string note_keychain_identifier =
      "3dbce93e-37a9-4c9f-aa6a-45812c484bc3";
  // Add password and note to the keychain.
  AddItemToKeychain(u"test1", password_keychain_identifier);
  AddItemToKeychain(u"password note", note_keychain_identifier);

  CreateDatabase("login_db_v38_with_keychain_id.sql");
  std::vector<std::unique_ptr<PasswordForm>> forms;
  {
    // Assert that the database was successfully opened and updated
    // to current version.
    base::HistogramTester histogram_tester;
    LoginDatabase db(get_database_path(), IsAccountStore(false));
    ASSERT_TRUE(db.Init());

    ExpectSuccessMetricsRecorded(histogram_tester, IsAccountStore(false));

    // Delete password from the keychain to check that GetAllLogins no longer
    // needs to access it.
    DeleteEncryptedPasswordFromKeychain(password_keychain_identifier);

    EXPECT_EQ(db.GetAllLogins(&forms), FormRetrievalResult::kSuccess);
    // Verify that |encrypted_password| is still corresponding to keychain
    // identifier.
    EXPECT_EQ(password_keychain_identifier, forms[0]->keychain_identifier);
    EXPECT_EQ(u"test1", forms[0]->password_value);
    // Verify that the password note is still readable.
    ASSERT_EQ(1u, forms[0]->notes.size());
    EXPECT_EQ(u"password note", forms[0]->notes[0].value);
  }
  {
    // Verify that password_value in the database is now encrypted with OSCrypt
    // and not equal to keychain identifier.
    std::vector<std::string> password_values(GetEncryptedPasswordValues());
    std::string expected_encrypted_password;
    ASSERT_EQ(1u, password_values.size());
    EXPECT_EQ(
        LoginDatabase::ENCRYPTION_RESULT_SUCCESS,
        LoginDatabase::EncryptedString(u"test1", &expected_encrypted_password));
    EXPECT_EQ(password_values[0], expected_encrypted_password);
  }

  // Clear item from the keychain to ensure this test doesn't affect other
  // tests.
  DeleteEncryptedPasswordFromKeychain(note_keychain_identifier);
}

TEST_P(LoginDatabaseMigrationToOSCryptTest,
       MigrationToVersion39SuccessMetricsAccountStore) {
  CreateDatabase("login_db_v38_with_keychain_id.sql");

  // The values set in the .sql file above are already in use by the previous
  // test. Since tests can run in parallel, the IDs need to be different to
  // avoid collisions. The following statements replace the existing IDs with
  // new ones.
  ReplacePasswordValue(
      "33353732613764632D353034362D343239622D623864342D33363936663837646339633"
      "2");

  // Sets the keychain id matching `note_keychain_identifier` so
  // that the lookup is successful when trying to migrate.
  ReplaceNoteValue(
      "39646263653933652D333761392D346339662D616136612D34353831326334383462633"
      "3");

  // Keychain identifiers matching the updated db IDs above.
  const std::string password_keychain_identifier =
      "3572a7dc-5046-429b-b8d4-3696f87dc9c2";
  const std::string note_keychain_identifier =
      "9dbce93e-37a9-4c9f-aa6a-45812c484bc3";

  // Add password and note to the keychain.
  AddItemToKeychain(u"test1", password_keychain_identifier);
  AddItemToKeychain(u"password note", note_keychain_identifier);

  // Assert that the database was successfully opened and updated
  // to current version.
  base::HistogramTester histogram_tester;
  LoginDatabase login_db(get_database_path(), IsAccountStore(true));
  ASSERT_TRUE(login_db.Init());

  ExpectSuccessMetricsRecorded(histogram_tester, IsAccountStore(true));

  // Delete password from the keychain to check that GetAllLogins no longer
  // needs to access it.
  DeleteEncryptedPasswordFromKeychain(password_keychain_identifier);

  // Clear item from the keychain to ensure this test doesn't affect other
  // tests.
  DeleteEncryptedPasswordFromKeychain(note_keychain_identifier);
}

TEST_P(LoginDatabaseMigrationToOSCryptTest,
       MigrationToVersion39WithMissingKeychainItems) {
  CreateDatabase("login_db_v38_with_keychain_ids.sql");

  // Even though testing file contains two passwords add only one item to the
  // keychain.
  const std::string password_keychain_identifier =
      "1e9bfa6c-d97d-45c2-90ef-5615c110a846";
  AddItemToKeychain(u"password", password_keychain_identifier);

  // Assert that the database was successfully opened and updated
  // to current version.
  base::HistogramTester histogram_tester;
  LoginDatabase login_db(get_database_path(), IsAccountStore(false));
  ASSERT_TRUE(login_db.Init());

  std::vector<std::unique_ptr<PasswordForm>> forms;
  EXPECT_EQ(login_db.GetAllLogins(&forms), FormRetrievalResult::kSuccess);
  EXPECT_EQ(1u, forms.size());
  EXPECT_EQ(u"password", forms[0]->password_value);

  ExpectSuccessMetricsRecorded(histogram_tester, IsAccountStore(false));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.MigrationToOSCrypt."
      "ProfileStore.DeletedPasswordCount",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.MigrationToOSCrypt."
      "ProfileStore.MigratedPasswordCount",
      1, 1);

  // Clear item from the keychain to ensure this test doesn't affect other
  // tests.
  DeleteEncryptedPasswordFromKeychain(password_keychain_identifier);
}

INSTANTIATE_TEST_SUITE_P(,  // Empty instantiation name.
                         LoginDatabaseMigrationToOSCryptTest,
                         testing::Bool());

}  // namespace password_manager
