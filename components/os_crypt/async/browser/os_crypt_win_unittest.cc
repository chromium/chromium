// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/os_crypt_win.h"

#include <windows.h>

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace os_crypt_async {

namespace {

constexpr char kOsCryptEncryptedKeyPrefName[] = "os_crypt.encrypted_key";
constexpr char kOsCryptAuditEnabledPrefName[] = "os_crypt.audit_enabled";
constexpr char kDPAPIKeyPrefix[] = "DPAPI";

}  // namespace

class OSCryptWinTest : public ::testing::Test {
 protected:
  void SetUp() override { RegisterLocalPrefs(prefs_.registry()); }

  TestingPrefServiceSimple prefs_;
};

TEST_F(OSCryptWinTest, Init) {
  // Initial call should generate a key.
  EXPECT_TRUE(Init(&prefs_));
  EXPECT_TRUE(prefs_.HasPrefPath(kOsCryptEncryptedKeyPrefName));
  EXPECT_TRUE(prefs_.GetBoolean(kOsCryptAuditEnabledPrefName));

  const std::string base64_encrypted_key =
      prefs_.GetString(kOsCryptEncryptedKeyPrefName);
  EXPECT_FALSE(base64_encrypted_key.empty());

  std::optional<std::vector<uint8_t>> encrypted_key =
      base::Base64Decode(base64_encrypted_key);
  ASSERT_TRUE(encrypted_key.has_value());
  EXPECT_TRUE(
      base::StartsWith(base::as_string_view(*encrypted_key), kDPAPIKeyPrefix));
  // Key should be at least prefix + some encrypted data.
  EXPECT_GT(encrypted_key->size(), sizeof(kDPAPIKeyPrefix) - 1);

  // Second call should not change the key.
  EXPECT_TRUE(Init(&prefs_));
  EXPECT_EQ(base64_encrypted_key,
            prefs_.GetString(kOsCryptEncryptedKeyPrefName));
}

TEST_F(OSCryptWinTest, InitWithExistingKey) {
  // Key doesn't exist yet.
  EXPECT_EQ(InitResult::kKeyDoesNotExist, InitWithExistingKey(&prefs_));

  // Generate a key.
  EXPECT_TRUE(Init(&prefs_));
  const std::string base64_encrypted_key =
      prefs_.GetString(kOsCryptEncryptedKeyPrefName);

  // Now InitWithExistingKey should succeed.
  EXPECT_EQ(InitResult::kSuccess, InitWithExistingKey(&prefs_));
  EXPECT_EQ(base64_encrypted_key,
            prefs_.GetString(kOsCryptEncryptedKeyPrefName));
}

TEST_F(OSCryptWinTest, ReencryptOnAuditDisabled) {
  EXPECT_TRUE(Init(&prefs_));

  // Manually disable audit.
  prefs_.SetBoolean(kOsCryptAuditEnabledPrefName, false);

  // Calling Init again should re-encrypt and enable audit.
  EXPECT_TRUE(Init(&prefs_));
  EXPECT_TRUE(prefs_.GetBoolean(kOsCryptAuditEnabledPrefName));

  // The key content should be the same, but the encrypted blob might be
  // different. Actually, our implementation of Init when audit is disabled
  // calls EncryptAndStoreKey with the same key.
  EXPECT_TRUE(prefs_.HasPrefPath(kOsCryptEncryptedKeyPrefName));
}

}  // namespace os_crypt_async
