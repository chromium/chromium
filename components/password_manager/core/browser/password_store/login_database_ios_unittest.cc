// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/login_database.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <stddef.h>

#include <string_view>
#include <tuple>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/common/passwords_directory_util_ios.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::Bucket;
using base::UTF16ToUTF8;
using base::apple::ScopedCFTypeRef;

namespace password_manager {

class LoginDatabaseIOSTest : public PlatformTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_oscrypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting(
        /*is_sync_for_unittests=*/true);
    base::FilePath login_db_path =
        temp_dir_.GetPath().AppendASCII("temp_login.db");
    login_db_.reset(new password_manager::LoginDatabase(
        login_db_path, password_manager::IsAccountStore(false)));
    login_db_->Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
                    /*encryptor=*/CreateEncryptor());
  }

  os_crypt_async::Encryptor CreateEncryptor() {
    base::test::TestFuture<os_crypt_async::Encryptor> future;

    test_oscrypt_async_->GetInstance(future.GetCallback(),
                                     os_crypt_async::Encryptor::Option::kNone);
    return future.Take();
  }

 protected:
  std::unique_ptr<os_crypt_async::OSCryptAsync> test_oscrypt_async_;
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
    EncryptDecryptInterface* encryptor_decryptor = login_db_.get();
    std::string encrypted;
    EXPECT_EQ(EncryptionResult::kSuccess,
              encryptor_decryptor->EncryptedString(
                  UNSAFE_TODO(test_passwords[i]), &encrypted));
    std::u16string decrypted;
    EXPECT_EQ(EncryptionResult::kSuccess,
              encryptor_decryptor->DecryptedString(encrypted, &decrypted));
    EXPECT_STREQ(UTF16ToUTF8(UNSAFE_TODO(test_passwords[i])).c_str(),
                 UTF16ToUTF8(decrypted).c_str());
  }
}

TEST_F(LoginDatabaseIOSTest, AddLogin) {
  StoredCredential cred;
  cred.url = GURL("http://0.com");
  cred.signon_realm = "http://www.example.com/";
  cred.action = GURL("http://www.example.com/action");
  cred.password_element = u"pwd";
  cred.password_value = u"example";

  std::u16string expected_password = cred.password_value;
  password_manager::PasswordStoreChangeList changes =
      login_db_->AddLogin(std::move(cred));
  std::string keychain_identifier = changes[0].credential().keychain_identifier;
  ASSERT_FALSE(keychain_identifier.empty());

  std::u16string password_value;
  EXPECT_EQ(errSecSuccess, GetTextFromKeychainIdentifier(keychain_identifier,
                                                         &password_value));
  EXPECT_EQ(expected_password, password_value);

  // Clear item from the keychain to ensure this test doesn't affect other
  // tests.
  DeleteEncryptedPasswordFromKeychain(keychain_identifier);
}

TEST_F(LoginDatabaseIOSTest, UpdateLogin) {
  StoredCredential cred;
  cred.url = GURL("http://0.com");
  cred.signon_realm = "http://www.example.com";
  cred.action = GURL("http://www.example.com/action");
  cred.password_element = u"pwd";
  cred.password_value = u"example";

  password_manager::PasswordStoreChangeList changes =
      login_db_->AddLogin(FromPasswordForm(ToPasswordForm(cred)));
  std::string old_keychain_identifier =
      changes[0].credential().keychain_identifier;

  cred.password_value = u"secret";

  ASSERT_THAT(login_db_->UpdateLogin(cred), testing::SizeIs(1));

  std::vector<StoredCredential> credentials;
  EXPECT_TRUE(login_db_->GetLogins(PasswordFormDigest(ToPasswordForm(cred)),
                                   true, &credentials));

  ASSERT_EQ(1U, credentials.size());
  std::string keychain_identifier = credentials[0].keychain_identifier;
  ASSERT_FALSE(keychain_identifier.empty());

  std::u16string password_value;
  EXPECT_EQ(errSecSuccess, GetTextFromKeychainIdentifier(keychain_identifier,
                                                         &password_value));
  EXPECT_EQ(cred.password_value, password_value);
  // Check that keychain item corresponding to the old password value is
  // deleted.
  EXPECT_EQ(errSecItemNotFound, GetTextFromKeychainIdentifier(
                                    old_keychain_identifier, &password_value));

  // Clear item from the keychain to ensure this test doesn't affect other
  // tests.
  DeleteEncryptedPasswordFromKeychain(keychain_identifier);
}

TEST_F(LoginDatabaseIOSTest, RemoveLogin) {
  StoredCredential cred;
  cred.signon_realm = "http://www.example.com";
  cred.url = GURL("http://www.example.com/action");
  cred.password_element = u"pwd";
  cred.password_value = u"example";

  password_manager::PasswordStoreChangeList changes =
      login_db_->AddLogin(FromPasswordForm(ToPasswordForm(cred)));
  std::string keychain_identifier = changes[0].credential().keychain_identifier;
  std::ignore = login_db_->RemoveLogin(cred, /*changes=*/nullptr);

  // Verify that password is no longer available in the keychain.
  std::u16string password_value;
  EXPECT_EQ(errSecItemNotFound, GetTextFromKeychainIdentifier(
                                    keychain_identifier, &password_value));
}

TEST_F(LoginDatabaseIOSTest, RemoveLoginsCreatedBetween) {
  StoredCredential creds[3];
  creds[0].url = GURL("http://0.com");
  creds[0].signon_realm = "http://www.example.com";
  creds[0].username_element = u"login0";
  creds[0].date_created = base::Time::FromSecondsSinceUnixEpoch(100);
  creds[0].password_value = u"pass0";
  creds[0].in_store = PasswordForm::Store::kProfileStore;

  creds[1].url = GURL("http://1.com");
  creds[1].signon_realm = "http://www.example.com";
  creds[1].username_element = u"login1";
  creds[1].date_created = base::Time::FromSecondsSinceUnixEpoch(200);
  creds[1].password_value = u"pass1";
  creds[1].in_store = PasswordForm::Store::kProfileStore;

  creds[2].url = GURL("http://2.com");
  creds[2].signon_realm = "http://www.example.com";
  creds[2].username_element = u"login2";
  creds[2].date_created = base::Time::FromSecondsSinceUnixEpoch(300);
  creds[2].password_value = u"pass2";
  creds[2].in_store = PasswordForm::Store::kProfileStore;

  PasswordForm expected_form_0 = ToPasswordForm(creds[0]);
  PasswordForm expected_form_2 = ToPasswordForm(creds[2]);

  for (auto& c : creds) {
    std::ignore = login_db_->AddLogin(std::move(c));
  }

  PasswordFormDigest form = {PasswordForm::Scheme::kHtml,
                             "http://www.example.com", GURL()};
  std::vector<StoredCredential> initial_credentials;
  EXPECT_TRUE(login_db_->GetLogins(form, true, &initial_credentials));
  ASSERT_EQ(3U, initial_credentials.size());
  // Verify that for each password exist a keychain item with a password.
  for (const auto& cred : initial_credentials) {
    std::u16string password_value;
    EXPECT_EQ(errSecSuccess, GetTextFromKeychainIdentifier(
                                 cred.keychain_identifier, &password_value));
    EXPECT_EQ(cred.password_value, password_value);
  }

  login_db_->RemoveLoginsCreatedBetween(
      base::Time::FromSecondsSinceUnixEpoch(150),
      base::Time::FromSecondsSinceUnixEpoch(250),
      /*changes=*/nullptr);

  // Verify that one password is removed.
  std::vector<StoredCredential> remaining_credentials;
  EXPECT_TRUE(login_db_->GetLogins(form, true, &remaining_credentials));
  EXPECT_THAT(ToPasswordForms(std::move(remaining_credentials)),
              testing::UnorderedElementsAreArray(
                  FormsIgnoringPrimaryKey({expected_form_0, expected_form_2})));

  // Verify that keychain entry is removed.
  std::u16string password_value;
  EXPECT_EQ(errSecSuccess,
            GetTextFromKeychainIdentifier(
                initial_credentials[0].keychain_identifier, &password_value));
  EXPECT_EQ(errSecItemNotFound,
            GetTextFromKeychainIdentifier(
                initial_credentials[1].keychain_identifier, &password_value));
  EXPECT_EQ(errSecSuccess,
            GetTextFromKeychainIdentifier(
                initial_credentials[2].keychain_identifier, &password_value));

  // Clear item from the keychain to ensure this test doesn't affect other
  // tests.
  DeleteEncryptedPasswordFromKeychain(
      initial_credentials[0].keychain_identifier);
  DeleteEncryptedPasswordFromKeychain(
      initial_credentials[2].keychain_identifier);
}

TEST_F(LoginDatabaseIOSTest, DeleteAndRecreateDatabaseFile) {
  StoredCredential creds[3];
  creds[0].url = GURL("http://0.com");
  creds[0].signon_realm = "http://www.example.com";
  creds[0].username_element = u"login0";
  creds[0].date_created = base::Time::FromSecondsSinceUnixEpoch(100);
  creds[0].password_value = u"pass0";
  creds[0].in_store = PasswordForm::Store::kProfileStore;

  creds[1].url = GURL("http://1.com");
  creds[1].signon_realm = "http://www.example.com";
  creds[1].username_element = u"login1";
  creds[1].date_created = base::Time::FromSecondsSinceUnixEpoch(200);
  creds[1].password_value = u"pass1";
  creds[1].in_store = PasswordForm::Store::kProfileStore;

  creds[2].url = GURL("http://2.com");
  creds[2].signon_realm = "http://www.example.com";
  creds[2].username_element = u"login2";
  creds[2].date_created = base::Time::FromSecondsSinceUnixEpoch(300);
  creds[2].password_value = u"pass2";
  creds[2].in_store = PasswordForm::Store::kProfileStore;

  for (auto& c : creds) {
    std::ignore = login_db_->AddLogin(std::move(c));
  }

  PasswordFormDigest form = {PasswordForm::Scheme::kHtml,
                             "http://www.example.com", GURL()};
  std::vector<StoredCredential> initial_credentials;
  EXPECT_TRUE(login_db_->GetLogins(form, true, &initial_credentials));
  ASSERT_EQ(3U, initial_credentials.size());
  // Verify that for each password exist a keychain item with a password.
  for (const auto& cred : initial_credentials) {
    std::u16string password_value;
    EXPECT_EQ(errSecSuccess, GetTextFromKeychainIdentifier(
                                 cred.keychain_identifier, &password_value));
    EXPECT_EQ(cred.password_value, password_value);
  }

  // Delete one keychain item to verify that nothing happens when trying to
  // delete it again.
  DeleteEncryptedPasswordFromKeychain(
      initial_credentials[0].keychain_identifier);

  base::HistogramTester histogram_tester;
  login_db_->DeleteAndRecreateDatabaseFile();

  // Verify that all passwords are gone.
  std::vector<StoredCredential> remaining_credentials;
  EXPECT_TRUE(login_db_->GetLogins(form, true, &remaining_credentials));
  EXPECT_THAT(remaining_credentials, testing::IsEmpty());

  // Verify that keychain entry is removed.
  std::u16string password_value;
  EXPECT_EQ(errSecItemNotFound,
            GetTextFromKeychainIdentifier(
                initial_credentials[0].keychain_identifier, &password_value));
  EXPECT_EQ(errSecItemNotFound,
            GetTextFromKeychainIdentifier(
                initial_credentials[1].keychain_identifier, &password_value));
  EXPECT_EQ(errSecItemNotFound,
            GetTextFromKeychainIdentifier(
                initial_credentials[2].keychain_identifier, &password_value));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "PasswordManager.LoginDatabase.DeleteFromKeychain"),
      BucketsInclude(Bucket(errSecSuccess, 2), Bucket(errSecItemNotFound, 1)));
}

class LoginDatabaseMigrationToOSCryptTest : public LoginDatabaseIOSTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_oscrypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting(
        /*is_sync_for_unittests=*/true);
    database_path_ = temp_dir_.GetPath().AppendASCII("test.db");
  }

  // Creates the database from |sql_file|.
  void CreateDatabase(std::string_view sql_file) {
    base::FilePath database_dump;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &database_dump));
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
    CFDictionarySetValue(attributes.get(), kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(attributes.get(), kSecAttrAccount, item_ref.get());
    std::string plain_text_utf8 = base::UTF16ToUTF8(value);
    ScopedCFTypeRef<CFDataRef> data(CFDataCreate(
        NULL, reinterpret_cast<const UInt8*>(plain_text_utf8.data()),
        plain_text_utf8.size()));
    CFDictionarySetValue(attributes.get(), kSecValueData, data.get());
    EXPECT_EQ(errSecSuccess, SecItemAdd(attributes.get(), NULL));
  }

  std::vector<std::string> GetEncryptedPasswordValues() const {
    sql::Database db(sql::test::kTestTag);
    CHECK(db.Open(database_path_));

    sql::Statement s(db.GetCachedStatement(
        SQL_FROM_HERE, "SELECT password_value FROM logins"));
    EXPECT_TRUE(s.is_valid());

    std::vector<std::string> results;
    while (s.Step()) {
      results.push_back(s.ColumnBlobAsString(0));
    }

    return results;
  }

  std::vector<std::string> GetEncryptedPasswordNoteValues() const {
    sql::Database db(sql::test::kTestTag);
    CHECK(db.Open(database_path_));

    sql::Statement s(db.GetCachedStatement(SQL_FROM_HERE,
                                           "SELECT value FROM password_notes"));
    EXPECT_TRUE(s.is_valid());

    std::vector<std::string> results;
    while (s.Step()) {
      results.push_back(s.ColumnBlobAsString(0));
    }

    return results;
  }

  void ReplacePasswordValue(const std::string& new_value) {
    sql::Database db(sql::test::kTestTag);
    CHECK(db.Open(get_database_path()));
    sql::Statement new_password_value(db.GetCachedStatement(
        SQL_FROM_HERE, "UPDATE logins SET password_value = ?"));
    new_password_value.BindString(0, new_value);
    ASSERT_TRUE(new_password_value.Run());
  }

  void ReplaceNoteValue(const std::string& new_value) {
    sql::Database db(sql::test::kTestTag);
    CHECK(db.Open(get_database_path()));
    sql::Statement new_note_value(db.GetCachedStatement(
        SQL_FROM_HERE, "UPDATE password_notes SET value = ?"));
    new_note_value.BindString(0, new_value);
    ASSERT_TRUE(new_note_value.Run());
  }

  base::FilePath get_database_path() { return database_path_; }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath database_path_;
};

// Tests the migration of the login database from version() to
// kCurrentVersionNumber.
TEST_F(LoginDatabaseMigrationToOSCryptTest,
       MigrationFromVersion38ProfileStore) {
  // Keychain identifier used in the test file.
  const std::string password_keychain_identifier =
      "2572a7dc-5046-429b-b8d4-3696f87dc9c2";
  const std::string note_keychain_identifier =
      "3dbce93e-37a9-4c9f-aa6a-45812c484bc3";
  // Add password and note to the keychain.
  AddItemToKeychain(u"test1", password_keychain_identifier);
  AddItemToKeychain(u"password note", note_keychain_identifier);

  CreateDatabase("login_db_v38_with_keychain_id.sql");
  {
    // Assert that the database was successfully opened and updated to current
    // version.
    base::HistogramTester histogram_tester;
    LoginDatabase db(get_database_path(), IsAccountStore(false));
    ASSERT_TRUE(
        db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
                /*encryptor=*/CreateEncryptor()));

    // Delete password from the keychain to check that GetAllLogins no longer
    // needs to access it.
    DeleteEncryptedPasswordFromKeychain(password_keychain_identifier);

    std::vector<StoredCredential> credentials;
    EXPECT_EQ(db.GetAllLogins(&credentials), FormRetrievalResult::kSuccess);
    ASSERT_EQ(credentials.size(), 1u);
    // Verify that |encrypted_password| is still corresponding to keychain
    // identifier.
    EXPECT_EQ(credentials[0].keychain_identifier, password_keychain_identifier);
    EXPECT_EQ(credentials[0].password_value, u"test1");
    // Verify that the password note is still readable.
    ASSERT_EQ(credentials[0].notes.size(), 1u);
    EXPECT_EQ(credentials[0].notes[0].value, u"password note");
  }
  {
    // Verify that password_value in the database is now encrypted with OSCrypt
    // and not equal to keychain identifier.
    std::vector<std::string> password_values(GetEncryptedPasswordValues());
    ASSERT_EQ(password_values.size(), 1u);
    std::string decrypted_password;
    ASSERT_TRUE(CreateEncryptor().DecryptString(password_values[0],
                                                &decrypted_password));
    EXPECT_EQ(decrypted_password, "test1");
  }

  // Clear item from the keychain to ensure this test doesn't affect other
  // tests.
  DeleteEncryptedPasswordFromKeychain(note_keychain_identifier);
}

TEST_F(LoginDatabaseMigrationToOSCryptTest,
       MigrationFromVersion38SuccessMetricsAccountStore) {
  CreateDatabase("login_db_v38_with_keychain_id.sql");

  // The values set in the .sql file above are already in use by the previous
  // test. Since tests can run in parallel, the IDs need to be different to
  // avoid collisions. The following statements replace the existing IDs with
  // new ones.
  ReplacePasswordValue(
      "33353732613764632D353034362D343239622D623864342D33363936663837646339633"
      "2");

  // Sets the keychain id matching `note_keychain_identifier` so that the lookup
  // is successful when trying to migrate.
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

  // Assert that the database was successfully opened and updated to current
  // version.
  base::HistogramTester histogram_tester;
  LoginDatabase login_db(get_database_path(), IsAccountStore(true));
  ASSERT_TRUE(
      login_db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
                    /*encryptor=*/CreateEncryptor()));

  // Delete password from the keychain to check that GetAllLogins no longer
  // needs to access it.
  DeleteEncryptedPasswordFromKeychain(password_keychain_identifier);

  // Clear item from the keychain to ensure this test doesn't affect other
  // tests.
  DeleteEncryptedPasswordFromKeychain(note_keychain_identifier);
}

TEST_F(LoginDatabaseMigrationToOSCryptTest,
       MigrationFromVersion38WithMissingKeychainItems) {
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
  ASSERT_TRUE(
      login_db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
                    /*encryptor=*/CreateEncryptor()));

  std::vector<StoredCredential> credentials;
  EXPECT_EQ(login_db.GetAllLogins(&credentials), FormRetrievalResult::kSuccess);
  ASSERT_EQ(1u, credentials.size());
  EXPECT_EQ(u"password", credentials[0].password_value);

  // Clear item from the keychain to ensure this test doesn't affect other
  // tests.
  DeleteEncryptedPasswordFromKeychain(password_keychain_identifier);
}

TEST_F(LoginDatabaseMigrationToOSCryptTest,
       MigrationFromVersion39ProfileStore) {
  // Keychain identifier used in the test file.
  const std::string note_keychain_identifier =
      "3dbcx93e-37a9-4c9f-aa6a-45812c484bc3";
  AddItemToKeychain(u"password note", note_keychain_identifier);

  CreateDatabase("login_db_v39_with_note_keychain_id.sql");
  {
    // Assert that the database was successfully opened and updated to current
    // version.
    base::HistogramTester histogram_tester;
    LoginDatabase login_db(get_database_path(), IsAccountStore(false));
    ASSERT_TRUE(login_db.Init(
        /*on_undecryptable_passwords_removed=*/base::NullCallback(),
        /*encryptor=*/CreateEncryptor()));

    // Delete note from the keychain to check that GetAllLogins no longer needs
    // to access it;
    DeleteEncryptedPasswordFromKeychain(note_keychain_identifier);

    std::vector<StoredCredential> credentials;
    EXPECT_EQ(login_db.GetAllLogins(&credentials),
              FormRetrievalResult::kSuccess);
    ASSERT_EQ(credentials.size(), 1u);
    // Verify that the password note is still readable.
    ASSERT_EQ(credentials[0].notes.size(), 1u);
    EXPECT_EQ(credentials[0].notes[0].value, u"password note");
  }
  {
    // Verify that note value in the database is now encrypted with OSCrypt and
    // not equal to keychain identifier.
    std::vector<std::string> note_values(GetEncryptedPasswordNoteValues());
    ASSERT_EQ(note_values.size(), 1u);
    std::u16string decrypted_note;
    ASSERT_TRUE(
        CreateEncryptor().DecryptString16(note_values[0], &decrypted_note));
    EXPECT_EQ(decrypted_note, u"password note");
  }
}

TEST_F(LoginDatabaseMigrationToOSCryptTest,
       MigrationFromVersion39AccountStore) {
  // The values set in the .sql file are already used by the previous test.
  // Since tests can run in parallel, the IDs nee to be different to avoid
  // collisions.
  const std::string note_keychain_identifier =
      "8dbcx93e-37a9-4c9f-aa6a-45812c484bc3";
  AddItemToKeychain(u"test_note", note_keychain_identifier);

  CreateDatabase("login_db_v39_with_note_keychain_id.sql");
  ReplaceNoteValue(note_keychain_identifier);

  std::vector<StoredCredential> credentials;
  {
    // Assert that the database was successfully opened and updated to current
    // version.
    base::HistogramTester histogram_tester;
    LoginDatabase login_db(get_database_path(), IsAccountStore(true));
    ASSERT_TRUE(login_db.Init(
        /*on_undecryptable_passwords_removed=*/base::NullCallback(),
        /*encryptor=*/CreateEncryptor()));

    // Delete note from the keychain to check that GetAllLogins no longer needs
    // to access it;
    DeleteEncryptedPasswordFromKeychain(note_keychain_identifier);

    EXPECT_EQ(login_db.GetAllLogins(&credentials),
              FormRetrievalResult::kSuccess);
    ASSERT_EQ(credentials.size(), 1u);
    // Verify that the password note is still readable.
    ASSERT_EQ(credentials[0].notes.size(), 1u);
    EXPECT_EQ(credentials[0].notes[0].value, u"test_note");
  }
  {
    // Verify that note value in the database is now encrypted with OSCrypt and
    // not equal to keychain identifier.
    std::vector<std::string> note_values(GetEncryptedPasswordNoteValues());
    ASSERT_EQ(note_values.size(), 1u);
    std::u16string decrypted_note;
    ASSERT_TRUE(
        CreateEncryptor().DecryptString16(note_values[0], &decrypted_note));
    EXPECT_EQ(decrypted_note, u"test_note");
  }
}

TEST_F(LoginDatabaseMigrationToOSCryptTest,
       MigrationFromVersion39WithMissingKeychainItems) {
  // Even though the testing file contains two notes, add only one of them to
  // the keychain to simulate the `errSecItemNotFound` error.
  const std::string note_keychain_identifier =
      "3dbcx93e-37a9-4c9f-aa6a-45812c484bc4";
  AddItemToKeychain(u"example_note", note_keychain_identifier);

  // Assert that the database was successfully opened and migrated.
  CreateDatabase("login_db_v39_with_note_keychain_ids.sql");
  base::HistogramTester histogram_tester;
  LoginDatabase login_db(get_database_path(), IsAccountStore(false));
  ASSERT_TRUE(
      login_db.Init(/*on_undecryptable_passwords_removed=*/base::NullCallback(),
                    /*encryptor=*/CreateEncryptor()));

  // Check that the first note is still readable and the second one was deleted
  // during migration.
  std::vector<StoredCredential> credentials;
  EXPECT_EQ(login_db.GetAllLogins(&credentials), FormRetrievalResult::kSuccess);
  ASSERT_EQ(credentials.size(), 2u);
  EXPECT_EQ(credentials[0].notes.size(), 1u);
  EXPECT_EQ(credentials[0].notes[0].value, u"example_note");
  EXPECT_EQ(credentials[1].notes.size(), 0u);

  // Clear the note from the keychain to ensure it doesn't affect other tests.
  DeleteEncryptedPasswordFromKeychain(note_keychain_identifier);
}

}  // namespace password_manager
