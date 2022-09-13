// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/login_database.h"

#import <Security/Security.h>
#include <stddef.h>

#include <memory>

#include "base/base64.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/common/passwords_directory_util_ios.h"
#include "sql/statement.h"

using base::ScopedCFTypeRef;

namespace password_manager {

// On iOS, the LoginDatabase uses Keychain API to store passwords. The
// "encrypted" version of the password is a unique ID (UUID) that is
// stored as an attribute along with the password in the keychain.
// A side effect of this approach is that the same password saved multiple
// times will have different "encrypted" values.

// TODO(ios): Use |Encryptor| to encrypt the login database. b/6976257

LoginDatabase::EncryptionResult LoginDatabase::EncryptedString(
    const std::u16string& plain_text,
    std::string* cipher_text) {
  if (plain_text.size() == 0) {
    *cipher_text = std::string();
    return ENCRYPTION_RESULT_SUCCESS;
  }

  ScopedCFTypeRef<CFUUIDRef> uuid(CFUUIDCreate(NULL));
  ScopedCFTypeRef<CFStringRef> item_ref(CFUUIDCreateString(NULL, uuid));
  ScopedCFTypeRef<CFMutableDictionaryRef> attributes(
      CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(attributes, kSecClass, kSecClassGenericPassword);

  // It does not matter which attribute we use to identify the keychain
  // item as long as it uniquely identifies it. We are arbitrarily choosing the
  // |kSecAttrAccount| attribute for this purpose.
  CFDictionarySetValue(attributes, kSecAttrAccount, item_ref);
  std::string plain_text_utf8 = base::UTF16ToUTF8(plain_text);
  ScopedCFTypeRef<CFDataRef> data(
      CFDataCreate(NULL, reinterpret_cast<const UInt8*>(plain_text_utf8.data()),
                   plain_text_utf8.size()));
  CFDictionarySetValue(attributes, kSecValueData, data);

  // Only allow access when the device has been unlocked.
  CFDictionarySetValue(attributes, kSecAttrAccessible,
                       kSecAttrAccessibleWhenUnlocked);

  OSStatus status = SecItemAdd(attributes, NULL);
  if (status != errSecSuccess) {
    NOTREACHED() << "Unable to save password in keychain: " << status;
    if (status == errSecDuplicateItem || status == errSecDecode)
      return ENCRYPTION_RESULT_ITEM_FAILURE;
    else
      return ENCRYPTION_RESULT_SERVICE_FAILURE;
  }

  *cipher_text = base::SysCFStringRefToUTF8(item_ref);
  return ENCRYPTION_RESULT_SUCCESS;
}

LoginDatabase::EncryptionResult LoginDatabase::DecryptedString(
    const std::string& cipher_text,
    std::u16string* plain_text) {
  if (cipher_text.size() == 0) {
    *plain_text = std::u16string();
    return ENCRYPTION_RESULT_SUCCESS;
  }

  ScopedCFTypeRef<CFStringRef> item_ref(
      base::SysUTF8ToCFStringRef(cipher_text));
  ScopedCFTypeRef<CFMutableDictionaryRef> query(
      CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);

  // We are using the account attribute to store item references.
  CFDictionarySetValue(query, kSecAttrAccount, item_ref);
  CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);

  CFDataRef data;
  OSStatus status = SecItemCopyMatching(query, (CFTypeRef*)&data);
  if (status != errSecSuccess) {
    OSSTATUS_LOG(INFO, status) << "Failed to retrieve password from keychain";
    if (status == errSecItemNotFound || status == errSecDecode)
      return ENCRYPTION_RESULT_ITEM_FAILURE;
    else
      return ENCRYPTION_RESULT_SERVICE_FAILURE;
  }

  const size_t size = CFDataGetLength(data);
  std::unique_ptr<UInt8[]> buffer(new UInt8[size]);
  CFDataGetBytes(data, CFRangeMake(0, size), buffer.get());
  CFRelease(data);

  *plain_text = base::UTF8ToUTF16(
      std::string(static_cast<char*>(static_cast<void*>(buffer.get())),
                  static_cast<size_t>(size)));
  return ENCRYPTION_RESULT_SUCCESS;
}

// static
void LoginDatabase::DeleteEncryptedPasswordFromKeychain(
    const std::string& cipher_text) {
  if (cipher_text.empty())
    return;

  ScopedCFTypeRef<CFMutableDictionaryRef> query(
      CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);

  ScopedCFTypeRef<CFStringRef> item_ref(
      base::SysUTF8ToCFStringRef(cipher_text));
  // We are using the account attribute to store item references.
  CFDictionarySetValue(query, kSecAttrAccount, item_ref);

  OSStatus status = SecItemDelete(query);
  if (status != errSecSuccess && status != errSecItemNotFound) {
    NOTREACHED() << "Unable to remove password from keychain: " << status;
  }

  // Delete the temporary passwords directory, since there might be leftover
  // temporary files used for password export that contain the password being
  // deleted. It can be called for a removal triggered by sync, which might
  // happen at the same time as an export operation. In the unlikely event
  // that the file is still needed by the consumer app, the export operation
  // will fail.
  password_manager::DeletePasswordsDirectory();
}

void LoginDatabase::DeleteEncryptedPasswordById(int id) {
  std::string cipher_text = GetEncryptedPasswordById(id);
  DeleteEncryptedPasswordFromKeychain(cipher_text);
}

std::string LoginDatabase::GetEncryptedPasswordById(int id) const {
  DCHECK(!encrypted_password_statement_by_id_.empty());
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE, encrypted_password_statement_by_id_.c_str()));

  s.BindInt(0, id);

  std::string encrypted_password;
  if (s.Step()) {
    s.ColumnBlobAsString(0, &encrypted_password);
  }
  return encrypted_password;
}

}  // namespace password_manager
