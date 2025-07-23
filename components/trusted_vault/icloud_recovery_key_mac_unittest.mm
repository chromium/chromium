// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/icloud_recovery_key_mac.h"

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/base64.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "crypto/apple/fake_keychain_v2.h"
#include "crypto/apple/keychain_v2.h"
#include "crypto/apple/scoped_fake_keychain_v2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using ::base::apple::CFToNSPtrCast;
using ::base::apple::NSToCFPtrCast;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Ne;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

constexpr char kKeychainAccessGroup[] = "keychain-access-group";
constexpr uint8_t kHeader[]{'h', 'e', 'a', 'd', 'e', 'r'};
constexpr uint8_t kPlaintext[]{'h', 'e', 'l', 'l', 'o'};

MATCHER_P(HasId, id, "") {
  if (!arg) {
    *result_listener << "Got null key.";
    return false;
  }
  return arg->id() == id;
}

class ICloudRecoveryKeyTest : public testing::Test {
 public:
  std::unique_ptr<ICloudRecoveryKey> CreateKey(
      const trusted_vault::SecurityDomainId security_domain_id) {
    base::test::TestFuture<std::unique_ptr<ICloudRecoveryKey>> new_key;
    ICloudRecoveryKey::Create(new_key.GetCallback(), security_domain_id,
                              kKeychainAccessGroup);
    return new_key.Take();
  }

  std::vector<std::unique_ptr<ICloudRecoveryKey>> RetrieveKeys(
      trusted_vault::SecurityDomainId security_domain_id,
      std::string_view keychain_access_group) {
    base::test::TestFuture<std::vector<std::unique_ptr<ICloudRecoveryKey>>>
        keys;
    ICloudRecoveryKey::Retrieve(keys.GetCallback(), security_domain_id,
                                keychain_access_group);
    return keys.Take();
  }

 protected:
  crypto::apple::ScopedFakeKeychainV2 fake_keychain_{kKeychainAccessGroup};
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ICloudRecoveryKeyTest, EndToEnd) {
  std::unique_ptr<ICloudRecoveryKey> key =
      CreateKey(trusted_vault::SecurityDomainId::kPasskeys);
  std::optional<std::vector<uint8_t>> encrypted =
      key->key()->public_key().Encrypt(base::span<uint8_t>(), kHeader,
                                       kPlaintext);
  ASSERT_THAT(encrypted, Ne(std::nullopt));

  std::vector<std::unique_ptr<ICloudRecoveryKey>> keys = RetrieveKeys(
      trusted_vault::SecurityDomainId::kPasskeys, kKeychainAccessGroup);
  ASSERT_THAT(keys, SizeIs(1u));

  std::optional<std::vector<uint8_t>> decrypted =
      keys[0]->key()->private_key().Decrypt(base::span<uint8_t>(), kHeader,
                                            *encrypted);
  EXPECT_THAT(decrypted, Optional(base::span<const uint8_t>(kPlaintext)));
}

TEST_F(ICloudRecoveryKeyTest, CreateAndRetrieve) {
  std::unique_ptr<ICloudRecoveryKey> key1 =
      CreateKey(trusted_vault::SecurityDomainId::kPasskeys);
  ASSERT_THAT(key1, NotNull());
  ASSERT_THAT(key1->key(), NotNull());

  std::unique_ptr<ICloudRecoveryKey> key2 =
      CreateKey(trusted_vault::SecurityDomainId::kPasskeys);
  ASSERT_THAT(key2, NotNull());
  ASSERT_THAT(key2->key(), NotNull());

  EXPECT_THAT(RetrieveKeys(trusted_vault::SecurityDomainId::kPasskeys,
                           kKeychainAccessGroup),
              UnorderedElementsAre(HasId(key1->id()), HasId(key2->id())));
}

// Verify that keys are stored using the new .hw_protected kSecAttrService, but
// old keys without it can still be retrieved.
TEST_F(ICloudRecoveryKeyTest, RetrieveWithLegacyAttributes) {
  std::unique_ptr<ICloudRecoveryKey> key1 =
      CreateKey(trusted_vault::SecurityDomainId::kPasskeys);
  ASSERT_THAT(key1, NotNull());
  ASSERT_THAT(key1->key(), NotNull());

  std::unique_ptr<ICloudRecoveryKey> key2 =
      CreateKey(trusted_vault::SecurityDomainId::kPasskeys);
  ASSERT_THAT(key2, NotNull());
  ASSERT_THAT(key2->key(), NotNull());

  CFMutableDictionaryRef key1_dict = const_cast<CFMutableDictionaryRef>(
      fake_keychain_.keychain()->items().at(0).get());
  auto service = base::apple::GetValueFromDictionary<CFStringRef>(
      key1_dict, kSecAttrService);
  EXPECT_EQ(base::SysCFStringRefToUTF8(service),
            "com.google.common.folsom.cloud.private.hw_protected");
  CFDictionarySetValue(
      key1_dict, kSecAttrService,
      base::SysUTF8ToCFStringRef("com.google.common.folsom.cloud.private")
          .get());

  EXPECT_THAT(RetrieveKeys(trusted_vault::SecurityDomainId::kPasskeys,
                           kKeychainAccessGroup),
              UnorderedElementsAre(HasId(key1->id()), HasId(key2->id())));
}

// Tests that keys belonging to other security domains are not retrieved.
TEST_F(ICloudRecoveryKeyTest, IgnoreOtherSecurityDomains) {
  std::unique_ptr<ICloudRecoveryKey> key1 =
      CreateKey(trusted_vault::SecurityDomainId::kPasskeys);
  ASSERT_THAT(key1, NotNull());
  ASSERT_THAT(key1->key(), NotNull());

  CFMutableDictionaryRef key1_dict = const_cast<CFMutableDictionaryRef>(
      fake_keychain_.keychain()->items().at(0).get());
  CFDictionarySetValue(key1_dict, kSecAttrService,
                       base::SysUTF8ToCFStringRef(
                           "com.google.common.folsom.cloud.private.folsom")
                           .get());

  EXPECT_THAT(RetrieveKeys(trusted_vault::SecurityDomainId::kPasskeys,
                           kKeychainAccessGroup),
              IsEmpty());
}

TEST_F(ICloudRecoveryKeyTest, MultipleSecurityDomains) {
  std::unique_ptr<ICloudRecoveryKey> passkeys_key =
      CreateKey(trusted_vault::SecurityDomainId::kPasskeys);
  ASSERT_THAT(passkeys_key, NotNull());
  ASSERT_THAT(passkeys_key->key(), NotNull());

  std::unique_ptr<ICloudRecoveryKey> chromesync_key =
      CreateKey(trusted_vault::SecurityDomainId::kChromeSync);
  ASSERT_THAT(chromesync_key, NotNull());
  ASSERT_THAT(chromesync_key->key(), NotNull());

  EXPECT_THAT(RetrieveKeys(trusted_vault::SecurityDomainId::kPasskeys,
                           kKeychainAccessGroup),
              ElementsAre(HasId(passkeys_key->id())));

  EXPECT_THAT(RetrieveKeys(trusted_vault::SecurityDomainId::kChromeSync,
                           kKeychainAccessGroup),
              ElementsAre(HasId(chromesync_key->id())));
}

TEST_F(ICloudRecoveryKeyTest, CreateKeychainError) {
  // Force a keychain error by setting the wrong access group.
  base::test::TestFuture<std::unique_ptr<ICloudRecoveryKey>> keys;
  ICloudRecoveryKey::Create(keys.GetCallback(),
                            trusted_vault::SecurityDomainId::kPasskeys,
                            "wrong keychain group");
  EXPECT_THAT(keys.Take(), IsNull());
}

TEST_F(ICloudRecoveryKeyTest, RetrieveKeychainError) {
  // Force a keychain error by setting the wrong access group.
  EXPECT_THAT(RetrieveKeys(trusted_vault::SecurityDomainId::kPasskeys,
                           "wrong keychain group"),
              IsEmpty());
}

TEST_F(ICloudRecoveryKeyTest, RetrieveEmpty) {
  EXPECT_THAT(RetrieveKeys(trusted_vault::SecurityDomainId::kPasskeys,
                           kKeychainAccessGroup),
              IsEmpty());
}

TEST_F(ICloudRecoveryKeyTest, RetrieveCorrupted) {
  std::unique_ptr<ICloudRecoveryKey> key1 =
      CreateKey(trusted_vault::SecurityDomainId::kPasskeys);
  ASSERT_THAT(key1, NotNull());
  ASSERT_THAT(key1->key(), NotNull());

  base::apple::ScopedCFTypeRef<CFDataRef> corrupted_key(
      CFDataCreate(kCFAllocatorDefault, nullptr, 0));
  CFDictionarySetValue(const_cast<CFMutableDictionaryRef>(
                           fake_keychain_.keychain()->items().at(0).get()),
                       kSecValueData, corrupted_key.get());

  EXPECT_THAT(RetrieveKeys(trusted_vault::SecurityDomainId::kPasskeys,
                           kKeychainAccessGroup),
              IsEmpty());
}

TEST_F(ICloudRecoveryKeyTest, RetrieveIOSKey) {
  std::unique_ptr<trusted_vault::SecureBoxKeyPair> key =
      trusted_vault::SecureBoxKeyPair::GenerateRandom();
  std::vector<uint8_t> private_key_bytes = key->private_key().ExportToBytes();

  // Manually create a key in exactly the way the iOS implementation creates it.
  // See crbug.com/416633274 for details and more background.
  NSDictionary* attributes = @{
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),
    CFToNSPtrCast(kSecAttrType) : @(static_cast<uint>('flsm')),
    CFToNSPtrCast(kSecAttrSynchronizable) : @YES,
    CFToNSPtrCast(kSecAttrService) :
        @"com.google.common.folsom.cloud.private.hw_protected",
    CFToNSPtrCast(kSecAttrAccessGroup) : @(kKeychainAccessGroup),
    CFToNSPtrCast(kSecAttrAccessible) :
        CFToNSPtrCast(kSecAttrAccessibleAfterFirstUnlock),
    CFToNSPtrCast(kSecAttrAccount) : base::SysUTF8ToNSString(
        base::Base64Encode((key->public_key().ExportToBytes()))),
    CFToNSPtrCast(kSecValueData) :
        [NSData dataWithBytes:private_key_bytes.data()
                       length:private_key_bytes.size()],
  };

  ASSERT_EQ(fake_keychain_.keychain()->ItemAdd(NSToCFPtrCast(attributes),
                                               /*result=*/nil),
            errSecSuccess);

  EXPECT_THAT(RetrieveKeys(trusted_vault::SecurityDomainId::kPasskeys,
                           kKeychainAccessGroup),
              ElementsAre(HasId(key->public_key().ExportToBytes())));
}

TEST_F(ICloudRecoveryKeyTest, RetrieveOldSecAttrAccessibleKey) {
  std::unique_ptr<trusted_vault::SecureBoxKeyPair> key =
      trusted_vault::SecureBoxKeyPair::GenerateRandom();
  std::vector<uint8_t> private_key_bytes = key->private_key().ExportToBytes();

  // Prior to crbug.com/416633274, keys were created slightly differently.
  // Verify that those keys can still be read.
  NSDictionary* attributes = @{
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),
    CFToNSPtrCast(kSecAttrType) : @(static_cast<uint>('flsm')),
    CFToNSPtrCast(kSecAttrSynchronizable) : @YES,
    CFToNSPtrCast(kSecAttrService) :
        @"com.google.common.folsom.cloud.private.hw_protected",
    CFToNSPtrCast(kSecAttrAccessGroup) : @(kKeychainAccessGroup),
    CFToNSPtrCast(kSecAttrAccessible) :
        CFToNSPtrCast(kSecAttrAccessibleWhenUnlocked),
    CFToNSPtrCast(kSecAttrAccount) : base::SysUTF8ToNSString(
        base::Base64Encode((key->public_key().ExportToBytes()))),
    CFToNSPtrCast(kSecValueData) :
        [NSData dataWithBytes:private_key_bytes.data()
                       length:private_key_bytes.size()],
  };

  ASSERT_EQ(fake_keychain_.keychain()->ItemAdd(NSToCFPtrCast(attributes),
                                               /*result=*/nil),
            errSecSuccess);

  EXPECT_THAT(RetrieveKeys(trusted_vault::SecurityDomainId::kPasskeys,
                           kKeychainAccessGroup),
              ElementsAre(HasId(key->public_key().ExportToBytes())));
}

}  // namespace

}  // namespace trusted_vault
