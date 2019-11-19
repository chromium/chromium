// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/login_database.h"

#include <Security/Security.h>
#include <stddef.h>

#include "base/files/scoped_temp_dir.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::ScopedCFTypeRef;
using base::UTF16ToUTF8;
using autofill::PasswordForm;

namespace password_manager {

class LoginDatabaseIOSTest : public PlatformTest {
 public:
  void SetUp() override {
    ClearKeychain();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath login_db_path =
        temp_dir_.GetPath().AppendASCII("temp_login.db");
    login_db_.reset(new password_manager::LoginDatabase(
        login_db_path, password_manager::IsAccountStore(false)));
    login_db_->Init();
  }

  void TearDown() override { ClearKeychain(); }

  // Removes all passwords from the keychain.  Since the unit test
  // executable does not share the keychain with anything else on iOS, clearing
  // the keychain will not affect any other applications.
  void ClearKeychain();

  // Returns the number of items in the keychain.
  size_t GetKeychainSize();

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<LoginDatabase> login_db_;
  base::test::TaskEnvironment task_environment_;
};

void LoginDatabaseIOSTest::ClearKeychain() {
  const void* queryKeys[] = {kSecClass};
  const void* queryValues[] = {kSecClassGenericPassword};
  ScopedCFTypeRef<CFDictionaryRef> query(CFDictionaryCreate(
      NULL, queryKeys, queryValues, base::size(queryKeys),
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
  OSStatus status = SecItemDelete(query);
  // iOS7 returns an error of |errSecItemNotFound| if you try to clear an empty
  // keychain.
  ASSERT_TRUE(status == errSecSuccess || status == errSecItemNotFound);
}

size_t LoginDatabaseIOSTest::GetKeychainSize() {
  // Verify that the keychain now contains exactly one item.
  ScopedCFTypeRef<CFMutableDictionaryRef> query(
      CFDictionaryCreateMutable(NULL, 4, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
  CFDictionarySetValue(query, kSecReturnAttributes, kCFBooleanTrue);
  CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
  CFDictionarySetValue(query, kSecAttrAccessible,
                       kSecAttrAccessibleWhenUnlocked);

  CFTypeRef result;
  OSStatus status = SecItemCopyMatching(query, &result);
  if (status == errSecItemNotFound)
    return 0;

  EXPECT_EQ(errSecSuccess, status);
  size_t size = CFArrayGetCount((CFArrayRef)result);
  CFRelease(result);
  return size;
}

TEST_F(LoginDatabaseIOSTest, KeychainStorage) {
  base::string16 test_passwords[] = {
      base::ASCIIToUTF16("foo"), base::ASCIIToUTF16("bar"),
      base::WideToUTF16(L"\u043F\u0430\u0440\u043E\u043B\u044C"),
      base::string16(),
  };

  for (unsigned int i = 0; i < base::size(test_passwords); i++) {
    std::string encrypted;
    EXPECT_EQ(LoginDatabase::ENCRYPTION_RESULT_SUCCESS,
              login_db_->EncryptedString(test_passwords[i], &encrypted));
    base::string16 decrypted;
    EXPECT_EQ(LoginDatabase::ENCRYPTION_RESULT_SUCCESS,
              login_db_->DecryptedString(encrypted, &decrypted));
    EXPECT_STREQ(UTF16ToUTF8(test_passwords[i]).c_str(),
                 UTF16ToUTF8(decrypted).c_str());
  }
}

TEST_F(LoginDatabaseIOSTest, UpdateLogin) {
  PasswordForm form;
  form.origin = GURL("http://0.com");
  form.signon_realm = "http://www.example.com";
  form.action = GURL("http://www.example.com/action");
  form.password_element = base::ASCIIToUTF16("pwd");
  form.password_value = base::ASCIIToUTF16("example");

  ignore_result(login_db_->AddLogin(form));

  form.password_value = base::ASCIIToUTF16("secret");

  password_manager::PasswordStoreChangeList changes =
      login_db_->UpdateLogin(form);
  ASSERT_EQ(1u, changes.size());

  std::vector<std::unique_ptr<PasswordForm>> forms;
  EXPECT_TRUE(login_db_->GetLogins(PasswordStore::FormDigest(form), &forms));

  ASSERT_EQ(1U, forms.size());
  EXPECT_STREQ("secret", UTF16ToUTF8(forms[0]->password_value).c_str());
  ASSERT_EQ(1U, GetKeychainSize());
}

TEST_F(LoginDatabaseIOSTest, RemoveLogin) {
  PasswordForm form;
  form.signon_realm = "www.example.com";
  form.action = GURL("www.example.com/action");
  form.password_element = base::ASCIIToUTF16("pwd");
  form.password_value = base::ASCIIToUTF16("example");

  ignore_result(login_db_->AddLogin(form));

  ignore_result(login_db_->RemoveLogin(form, /*changes=*/nullptr));

  std::vector<std::unique_ptr<PasswordForm>> forms;
  EXPECT_TRUE(login_db_->GetLogins(PasswordStore::FormDigest(form), &forms));

  ASSERT_EQ(0U, forms.size());
  ASSERT_EQ(0U, GetKeychainSize());
}

TEST_F(LoginDatabaseIOSTest, RemoveLoginsCreatedBetween) {
  PasswordForm forms[3];
  forms[0].origin = GURL("http://0.com");
  forms[0].signon_realm = "http://www.example.com";
  forms[0].username_element = base::ASCIIToUTF16("login0");
  forms[0].date_created = base::Time::FromDoubleT(100);
  forms[0].password_value = base::ASCIIToUTF16("pass0");

  forms[1].origin = GURL("http://1.com");
  forms[1].signon_realm = "http://www.example.com";
  forms[1].username_element = base::ASCIIToUTF16("login1");
  forms[1].date_created = base::Time::FromDoubleT(200);
  forms[1].password_value = base::ASCIIToUTF16("pass1");

  forms[2].origin = GURL("http://2.com");
  forms[2].signon_realm = "http://www.example.com";
  forms[2].username_element = base::ASCIIToUTF16("login2");
  forms[2].date_created = base::Time::FromDoubleT(300);
  forms[2].password_value = base::ASCIIToUTF16("pass2");

  for (size_t i = 0; i < base::size(forms); i++) {
    ignore_result(login_db_->AddLogin(forms[i]));
  }

  login_db_->RemoveLoginsCreatedBetween(base::Time::FromDoubleT(150),
                                        base::Time::FromDoubleT(250),
                                        /*changes=*/nullptr);

  PasswordStore::FormDigest form = {PasswordForm::Scheme::kHtml,
                                    "http://www.example.com", GURL()};
  std::vector<std::unique_ptr<PasswordForm>> logins;
  EXPECT_TRUE(login_db_->GetLogins(form, &logins));

  ASSERT_EQ(2U, logins.size());
  ASSERT_EQ(2U, GetKeychainSize());

  EXPECT_STREQ("login0", UTF16ToUTF8(logins[0]->username_element).c_str());
  EXPECT_STREQ("pass0", UTF16ToUTF8(logins[0]->password_value).c_str());
  EXPECT_STREQ("login2", UTF16ToUTF8(logins[1]->username_element).c_str());
  EXPECT_STREQ("pass2", UTF16ToUTF8(logins[1]->password_value).c_str());
}

}  // namespace password_manager
