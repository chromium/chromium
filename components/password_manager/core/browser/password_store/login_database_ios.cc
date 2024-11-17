// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/login_database.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Security/Security.h>
#include <stddef.h>

#include <memory>

#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/base64.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/password_manager/core/common/passwords_directory_util_ios.h"
#include "sql/statement.h"

using base::apple::ScopedCFTypeRef;

namespace {
// Retrieval from keychain may fail unexpectedly. e.g. if the keychain
// identifier that Chrome has is incorrect. This constant is not among error
// codes that can be returned by the keychain.
constexpr int kUnknownRetrievalError = -1;
}  // namespace

namespace password_manager {

// On iOS, the LoginDatabase uses Keychain API to store passwords. The
// "encrypted" version of the password is a unique ID (UUID) that is
// stored as an attribute along with the password in the keychain.
// A side effect of this approach is that the same password saved multiple
// times will have different "encrypted" values.
EncryptionResult LoginDatabase::EncryptedString(
    const std::u16string& plain_text,
    std::string* cipher_text) const {
  bool result = encryptor_
                    ? encryptor_->EncryptString16(plain_text, cipher_text)
                    : OSCrypt::EncryptString16(plain_text, cipher_text);
  return result ? EncryptionResult::kSuccess
                : EncryptionResult::kServiceFailure;
}

EncryptionResult LoginDatabase::DecryptedString(
    const std::string& cipher_text,
    std::u16string* plain_text) const {
  bool result = encryptor_
                    ? encryptor_->DecryptString16(cipher_text, plain_text)
                    : OSCrypt::DecryptString16(cipher_text, plain_text);
  return result ? EncryptionResult::kSuccess
                : EncryptionResult::kServiceFailure;
}

bool CreateKeychainIdentifier(const std::u16string& plain_text,
                              std::string* keychain_identifier) {
  if (plain_text.size() == 0) {
    *keychain_identifier = std::string();
    return true;
  }

  ScopedCFTypeRef<CFUUIDRef> uuid(CFUUIDCreate(NULL));
  ScopedCFTypeRef<CFStringRef> item_ref(CFUUIDCreateString(NULL, uuid.get()));
  ScopedCFTypeRef<CFMutableDictionaryRef> attributes(
      CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(attributes.get(), kSecClass, kSecClassGenericPassword);

  // It does not matter which attribute we use to identify the keychain
  // item as long as it uniquely identifies it. We are arbitrarily choosing the
  // |kSecAttrAccount| attribute for this purpose.
  CFDictionarySetValue(attributes.get(), kSecAttrAccount, item_ref.get());
  std::string plain_text_utf8 = base::UTF16ToUTF8(plain_text);
  ScopedCFTypeRef<CFDataRef> data(
      CFDataCreate(NULL, reinterpret_cast<const UInt8*>(plain_text_utf8.data()),
                   plain_text_utf8.size()));
  CFDictionarySetValue(attributes.get(), kSecValueData, data.get());

  // Only allow access when the device has been unlocked.
  CFDictionarySetValue(attributes.get(), kSecAttrAccessible,
                       kSecAttrAccessibleWhenUnlocked);

  OSStatus status = SecItemAdd(attributes.get(), NULL);
  if (status != errSecSuccess) {
    // TODO(crbug.com/40697564): This was a NOTREACHED() that would trigger when
    // sync runs on a locked device. When the linked bug is resolved it may be
    // possible to turn the LOG(ERROR) back into a NOTREACHED().
    LOG(ERROR) << "Unable to save password in keychain: " << status;
    return false;
  }

  *keychain_identifier = base::SysCFStringRefToUTF8(item_ref.get());
  return true;
}

OSStatus GetTextFromKeychainIdentifier(const std::string& keychain_identifier,
                                       std::u16string* plain_text) {
  if (keychain_identifier.size() == 0) {
    *plain_text = std::u16string();
    return errSecSuccess;
  }

  ScopedCFTypeRef<CFStringRef> item_ref(
      base::SysUTF8ToCFStringRef(keychain_identifier));
  if (!item_ref) {
    return kUnknownRetrievalError;
  }
  ScopedCFTypeRef<CFMutableDictionaryRef> query(
      CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(query.get(), kSecClass, kSecClassGenericPassword);

  // We are using the account attribute to store item references.
  CFDictionarySetValue(query.get(), kSecAttrAccount, item_ref.get());
  CFDictionarySetValue(query.get(), kSecReturnData, kCFBooleanTrue);

  ScopedCFTypeRef<CFTypeRef> data_cftype;
  OSStatus status =
      SecItemCopyMatching(query.get(), data_cftype.InitializeInto());
  if (status != errSecSuccess) {
    OSSTATUS_LOG(INFO, status) << "Failed to retrieve password from keychain";
    return status;
  }

  CFDataRef data = base::apple::CFCast<CFDataRef>(data_cftype.get());
  *plain_text =
      base::UTF8ToUTF16(base::as_string_view(base::apple::CFDataToSpan(data)));
  return errSecSuccess;
}

void DeleteEncryptedPasswordFromKeychain(
    const std::string& keychain_identifier) {
  if (keychain_identifier.empty()) {
    return;
  }

  ScopedCFTypeRef<CFMutableDictionaryRef> query(
      CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(query.get(), kSecClass, kSecClassGenericPassword);

  ScopedCFTypeRef<CFStringRef> item_ref(
      base::SysUTF8ToCFStringRef(keychain_identifier));
  // We are using the account attribute to store item references.
  CFDictionarySetValue(query.get(), kSecAttrAccount, item_ref.get());

  OSStatus status = SecItemDelete(query.get());
  base::UmaHistogramSparse("PasswordManager.LoginDatabase.DeleteFromKeychain",
                           static_cast<int>(status));

  // Delete the temporary passwords directory, since there might be leftover
  // temporary files used for password export that contain the password being
  // deleted. It can be called for a removal triggered by sync, which might
  // happen at the same time as an export operation. In the unlikely event
  // that the file is still needed by the consumer app, the export operation
  // will fail.
  password_manager::DeletePasswordsDirectory();
}

OSStatus GetAllPasswordsFromKeychain(
    std::unordered_map<std::string, std::u16string>* key_password_pairs) {
  CHECK(key_password_pairs);
  ScopedCFTypeRef<CFMutableDictionaryRef> query(
      CFDictionaryCreateMutable(NULL, 4, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(query.get(), kSecClass, kSecClassGenericPassword);
  CFDictionarySetValue(query.get(), kSecReturnAttributes, kCFBooleanTrue);
  CFDictionarySetValue(query.get(), kSecMatchLimit, kSecMatchLimitAll);
  CFDictionarySetValue(query.get(), kSecAttrAccessible,
                       kSecAttrAccessibleWhenUnlocked);
  CFDictionarySetValue(query.get(), kSecReturnData, kCFBooleanTrue);

  ScopedCFTypeRef<CFTypeRef> result;
  OSStatus status = SecItemCopyMatching(query.get(), result.InitializeInto());
  if (status != errSecSuccess) {
    return status;
  }
  CFArrayRef results = base::apple::CFCast<CFArrayRef>(result.get());
  const CFIndex count = CFArrayGetCount(results);
  for (CFIndex i = 0; i < count; ++i) {
    CFDictionaryRef dict = base::apple::CFCast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(results, i));
    std::string key = base::SysCFStringRefToUTF8(
        base::apple::GetValueFromDictionary<CFStringRef>(dict,
                                                         kSecAttrAccount));

    if (CFDataRef data = base::apple::GetValueFromDictionary<CFDataRef>(
            dict, kSecValueData)) {
      std::u16string plain_text = base::UTF8ToUTF16(
          base::as_string_view(base::apple::CFDataToSpan(data)));
      key_password_pairs->emplace(key, std::move(plain_text));
    }
  }
  return errSecSuccess;
}

}  // namespace password_manager
