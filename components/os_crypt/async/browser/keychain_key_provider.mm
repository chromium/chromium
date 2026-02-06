// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/keychain_key_provider.h"

#include <array>
#include <memory>

#include "base/apple/osstatus_logging.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/common/keychain_password_mac.h"
#include "components/os_crypt/common/os_crypt_switches.h"
#include "crypto/apple/fake_keychain_v2.h"
#include "crypto/apple/keychain_v2.h"
#include "crypto/kdf.h"
#include "crypto/subtle_passkey.h"

namespace os_crypt_async {

namespace {

// These constants are duplicated from the sync backend os_crypt_mac.mm.
constexpr char kKeyTag[] = "v10";
constexpr size_t kDerivedKeySize = 16;
constexpr auto kSalt =
    std::to_array<uint8_t>({'s', 'a', 'l', 't', 'y', 's', 'a', 'l', 't'});
constexpr size_t kIterations = 1003;

// This function runs on a worker thread and performs blocking Keychain IO.
base::expected<Encryptor::Key, KeyProvider::KeyError> GetKeyTask(
    crypto::SubtlePassKey subtle_passkey,
    crypto::apple::KeychainV2* keychain_for_testing) {
  std::unique_ptr<crypto::apple::FakeKeychainV2> scoped_fake_keychain;
  crypto::apple::KeychainV2* keychain_to_use = keychain_for_testing;
  if (!keychain_to_use) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            os_crypt::switches::kUseMockKeychain)) {
      scoped_fake_keychain =
          std::make_unique<crypto::apple::FakeKeychainV2>("test-access-group");
      keychain_to_use = scoped_fake_keychain.get();
    } else {
      keychain_to_use = &crypto::apple::KeychainV2::GetInstance();
    }
  }

  KeychainPassword keychain_password(*keychain_to_use);
  std::string password = keychain_password.GetPassword();

  // `password` can be an empty string if keychain access is denied by the user
  // or some other error occurs.
  if (password.empty()) {
    return base::unexpected(KeyProvider::KeyError::kTemporarilyUnavailable);
  }

  std::array<uint8_t, kDerivedKeySize> key_bytes;
  crypto::kdf::DeriveKeyPbkdf2HmacSha1({.iterations = kIterations},
                                       base::as_byte_span(password), kSalt,
                                       key_bytes, subtle_passkey);

  return Encryptor::Key(key_bytes, mojom::Algorithm::kAES128CBC);
}

}  // namespace

KeychainKeyProvider::KeychainKeyProvider() = default;

KeychainKeyProvider::KeychainKeyProvider(
    crypto::apple::KeychainV2* keychain_for_testing)
    : keychain_for_testing_(keychain_for_testing) {}

KeychainKeyProvider::~KeychainKeyProvider() = default;

void KeychainKeyProvider::GetKey(KeyCallback callback) {
  // Apple's documentation [1] recommends accessing the keychain on a worker
  // thread. [1]
  // https://developer.apple.com/documentation/security/secitemcopymatching%28_%3A_%3A%29#Performance-considerations
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(&GetKeyTask, crypto::SubtlePassKey{},
                     keychain_for_testing_),
      base::BindOnce(std::move(callback), kKeyTag));
}

bool KeychainKeyProvider::UseForEncryption() {
  return true;
}

bool KeychainKeyProvider::IsCompatibleWithOsCryptSync() {
  return true;
}

}  // namespace os_crypt_async
