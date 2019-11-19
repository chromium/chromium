// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/os_crypt.h"

#include <CommonCrypto/CommonCryptor.h>  // for kCCBlockSizeAES128
#include <stddef.h>

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "components/os_crypt/keychain_password_mac.h"
#include "components/os_crypt/os_crypt_switches.h"
#include "crypto/apple_keychain.h"
#include "crypto/encryptor.h"
#include "crypto/mock_apple_keychain.h"
#include "crypto/symmetric_key.h"

#if defined(OS_IOS)
#include "components/os_crypt/encryption_key_creation_util_ios.h"
#else
#include "base/threading/thread_task_runner_handle.h"
#include "components/os_crypt/encryption_key_creation_util_mac.h"
#include "components/os_crypt/os_crypt_pref_names_mac.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#endif

using crypto::AppleKeychain;

namespace os_crypt {
class EncryptionKeyCreationUtil;
}

namespace {

// Salt for Symmetric key derivation.
const char kSalt[] = "saltysalt";

// Key size required for 128 bit AES.
const size_t kDerivedKeySizeInBits = 128;

// Constant for Symmetic key derivation.
const size_t kEncryptionIterations = 1003;

// TODO(dhollowa): Refactor to allow dependency injection of Keychain.
bool use_mock_keychain = false;

// This flag is used to make the GetEncryptionKey method return NULL if used
// along with mock Keychain.
bool use_locked_mock_keychain = false;

// Prefix for cypher text returned by current encryption version.  We prefix
// the cypher text with this string so that future data migration can detect
// this and migrate to different encryption without data loss.
const char kEncryptionVersionPrefix[] = "v10";

// A utility which prevents overwriting the encryption key. This is temporary
// pointer that is non-NULL between initialization and getting the encryption
// key for the first time.
os_crypt::EncryptionKeyCreationUtil* g_key_creation_util = nullptr;

// This lock is used to make the GetEncrytionKey and
// OSCrypt::GetRawEncryptionKey methods thread-safe.
base::LazyInstance<base::Lock>::Leaky g_lock = LAZY_INSTANCE_INITIALIZER;

// The cached AES encryption key singleton.
crypto::SymmetricKey* g_cached_encryption_key = nullptr;

// true if |g_cached_encryption_key| has been initialized.
bool g_key_is_cached = false;

// Generates a newly allocated SymmetricKey object based on the password found
// in the Keychain.  The generated key is for AES encryption.  Returns NULL key
// in the case password access is denied or key generation error occurs.
crypto::SymmetricKey* GetEncryptionKey() {
  base::AutoLock auto_lock(g_lock.Get());

  if (use_mock_keychain && use_locked_mock_keychain)
    return nullptr;

  if (g_key_is_cached)
    return g_cached_encryption_key;

  static bool mock_keychain_command_line_flag =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          os_crypt::switches::kUseMockKeychain);

  std::string password;
  if (use_mock_keychain || mock_keychain_command_line_flag) {
    crypto::MockAppleKeychain keychain;
    password = keychain.GetEncryptionPassword();
  } else {
#if defined(OS_IOS)
    DCHECK(!g_key_creation_util);
    g_key_creation_util = new os_crypt::EncryptionKeyCreationUtilIOS();
#endif
    DCHECK(g_key_creation_util);
    AppleKeychain keychain;
    KeychainPassword encryptor_password(
        keychain,
        std::unique_ptr<EncryptionKeyCreationUtil>(g_key_creation_util));
    password = encryptor_password.GetPassword();
    g_key_creation_util = nullptr;
  }

  // Subsequent code must guarantee that the correct key is cached before
  // returning.
  g_key_is_cached = true;

  if (password.empty())
    return g_cached_encryption_key;

  std::string salt(kSalt);

  // Create an encryption key from our password and salt. The key is
  // intentionally leaked.
  g_cached_encryption_key =
      crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
          crypto::SymmetricKey::AES, password, salt, kEncryptionIterations,
          kDerivedKeySizeInBits)
          .release();
  ANNOTATE_LEAKING_OBJECT_PTR(g_cached_encryption_key);
  DCHECK(g_cached_encryption_key);
  return g_cached_encryption_key;
}

}  // namespace

// static
std::string OSCrypt::GetRawEncryptionKey() {
  crypto::SymmetricKey* key = GetEncryptionKey();
  if (!key)
    return std::string();
  return key->key();
}

// static
void OSCrypt::SetRawEncryptionKey(const std::string& raw_key) {
  base::AutoLock auto_lock(g_lock.Get());
  DCHECK(!g_key_is_cached) << "Encryption key already set.";
  if (!raw_key.empty()) {
    auto key = crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, raw_key);
    g_cached_encryption_key = key.release();
  }
  g_key_is_cached = true;
}

bool OSCrypt::EncryptString16(const base::string16& plaintext,
                              std::string* ciphertext) {
  return EncryptString(base::UTF16ToUTF8(plaintext), ciphertext);
}

bool OSCrypt::DecryptString16(const std::string& ciphertext,
                              base::string16* plaintext) {
  std::string utf8;
  if (!DecryptString(ciphertext, &utf8))
    return false;

  *plaintext = base::UTF8ToUTF16(utf8);
  return true;
}

bool OSCrypt::EncryptString(const std::string& plaintext,
                            std::string* ciphertext) {
  if (plaintext.empty()) {
    *ciphertext = std::string();
    return true;
  }

  crypto::SymmetricKey* encryption_key = GetEncryptionKey();
  if (!encryption_key)
    return false;

  std::string iv(kCCBlockSizeAES128, ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(encryption_key, crypto::Encryptor::CBC, iv))
    return false;

  if (!encryptor.Encrypt(plaintext, ciphertext))
    return false;

  // Prefix the cypher text with version information.
  ciphertext->insert(0, kEncryptionVersionPrefix);
  return true;
}

bool OSCrypt::DecryptString(const std::string& ciphertext,
                            std::string* plaintext) {
  if (ciphertext.empty()) {
    *plaintext = std::string();
    return true;
  }

  // Check that the incoming cyphertext was indeed encrypted with the expected
  // version.  If the prefix is not found then we'll assume we're dealing with
  // old data saved as clear text and we'll return it directly.
  // Credit card numbers are current legacy data, so false match with prefix
  // won't happen.
  if (ciphertext.find(kEncryptionVersionPrefix) != 0) {
    *plaintext = ciphertext;
    return true;
  }

  // Strip off the versioning prefix before decrypting.
  std::string raw_ciphertext =
      ciphertext.substr(strlen(kEncryptionVersionPrefix));

  crypto::SymmetricKey* encryption_key = GetEncryptionKey();
  if (!encryption_key) {
    VLOG(1) << "Decryption failed: could not get the key";
    return false;
  }

  std::string iv(kCCBlockSizeAES128, ' ');
  crypto::Encryptor encryptor;
  if (!encryptor.Init(encryption_key, crypto::Encryptor::CBC, iv))
    return false;

  if (!encryptor.Decrypt(raw_ciphertext, plaintext)) {
    VLOG(1) << "Decryption failed";
    return false;
  }

  return true;
}

bool OSCrypt::IsEncryptionAvailable() {
  return GetEncryptionKey() != nullptr;
}

#if !defined(OS_IOS)
void OSCrypt::RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(os_crypt::prefs::kKeyCreated, false);
}

bool OSCrypt::Init(PrefService* local_state) {
  base::AutoLock auto_lock(g_lock.Get());
  g_key_creation_util = new os_crypt::EncryptionKeyCreationUtilMac(
      local_state, base::ThreadTaskRunnerHandle::Get());
  return true;
}
#endif

void OSCrypt::UseMockKeychainForTesting(bool use_mock) {
  use_mock_keychain = use_mock;
  if (!use_mock_keychain)
    use_locked_mock_keychain = false;
}

void OSCrypt::UseLockedMockKeychainForTesting(bool use_locked) {
  use_locked_mock_keychain = use_locked;
  if (use_locked_mock_keychain)
    use_mock_keychain = true;
}
