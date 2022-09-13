// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"

#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private-join-and-compute/src/crypto/context.h"
#include "third_party/private-join-and-compute/src/crypto/ec_commutative_cipher.h"
#include "third_party/private-join-and-compute/src/crypto/ec_group.h"
#include "third_party/private-join-and-compute/src/crypto/ec_point.h"

namespace password_manager {
namespace {

// Returns the hash of |plaintext| used by the encryption algorithm.
std::string CalculateECCurveHash(const std::string& plaintext) {
  using ::private_join_and_compute::Context;
  using ::private_join_and_compute::ECGroup;

  std::unique_ptr<Context> context(new Context);
  auto group = ECGroup::Create(NID_X9_62_prime256v1, context.get());
  auto point = group.value().GetPointByHashingToCurveSha256(plaintext);
  return point.value().ToBytesCompressed().value();
}

// Converts a string to an array for printing.
std::vector<int> StringAsArray(const std::string& s) {
  return std::vector<int>(s.begin(), s.end());
}

}  // namespace

using ::testing::ElementsAreArray;

TEST(EncryptionUtils, CanonicalizeUsername) {
  // Ignore capitalization and mail hosts.
  EXPECT_EQ("test", CanonicalizeUsername("test"));
  EXPECT_EQ("test", CanonicalizeUsername("Test"));
  EXPECT_EQ("test", CanonicalizeUsername("TEST"));
  EXPECT_EQ("test", CanonicalizeUsername("test@mail.com"));
  EXPECT_EQ("test", CanonicalizeUsername("TeSt@MaIl.cOm"));
  EXPECT_EQ("test", CanonicalizeUsername("TEST@MAIL.COM"));

  // Strip off dots.
  EXPECT_EQ("foobar", CanonicalizeUsername("foo.bar@COM"));

  // Keep all but the last '@' sign.
  EXPECT_EQ("te@st", CanonicalizeUsername("te@st@mail.com"));
}

TEST(EncryptionUtils, HashUsername) {
  // Same test case as used by the server-side implementation:
  // go/passwords-leak-test
  constexpr uint8_t kExpected[] = {
      0x3D, 0x70, 0xD3, 0x7B, 0xFC, 0x1A, 0x3D, 0x81, 0x45, 0xE6, 0xC7,
      0xA3, 0xA4, 0xD7, 0x92, 0x76, 0x61, 0xC1, 0xE8, 0xDF, 0x82, 0xBD,
      0x0C, 0x9F, 0x61, 0x9A, 0xA3, 0xC9, 0x96, 0xEC, 0x4C, 0xB3};
  EXPECT_THAT(HashUsername("jonsnow"),
              ElementsAreArray(reinterpret_cast<const char*>(kExpected),
                               std::size(kExpected)));
}

TEST(EncryptionUtils, BucketizeUsername) {
  EXPECT_THAT(BucketizeUsername("jonsnow"),
              ElementsAreArray({0x3D, 0x70, 0xD3, 0x40}));
}

TEST(EncryptionUtils, ScryptHashUsernameAndPassword) {
  // The expected result was obtained by running the Java implementation of the
  // hash.
  // Needs to stay in sync with server side constant: go/passwords-leak-salts.
  constexpr uint8_t kExpected[] = {
      0x99, 0x7E, 0xF6, 0x76, 0x07, 0x4C, 0xCD, 0xB4, 0xC8, 0xAE, 0xDA,
      0x1F, 0x72, 0x3D, 0xF9, 0x67, 0x4C, 0x5B, 0x34, 0xCC, 0x2F, 0xEA,
      0x6B, 0x4D, 0x76, 0x7B, 0xF2, 0x83, 0x85, 0x55, 0x73, 0xFD};
  std::string result = *ScryptHashUsernameAndPassword("user", "password123");
  EXPECT_THAT(result, ElementsAreArray(reinterpret_cast<const char*>(kExpected),
                                       std::size(kExpected)));
}

TEST(EncryptionUtils, EncryptAndDecrypt) {
  constexpr char kRandomString[] = "very_secret";
  std::string key;
  std::string cipher = *CipherEncrypt(kRandomString, &key);
  SCOPED_TRACE(testing::Message()
               << "key=" << testing::PrintToString(StringAsArray(key)));

  EXPECT_NE(kRandomString, cipher);
  EXPECT_NE(std::string(), key);
  EXPECT_THAT(CalculateECCurveHash(kRandomString),
              ElementsAreArray(*CipherDecrypt(cipher, key)));
}

TEST(EncryptionUtils, EncryptAndDecryptWithPredefinedKey) {
  constexpr char kRandomString[] = "very_secret";
  constexpr uint8_t kKey[] = {0xFD, 0xB0, 0x2C, 0x8F, 0xFF, 0xBD, 0x31, 0x88,
                              0xA5, 0x36, 0xF1, 0xFE, 0x0D, 0xA9, 0x5F, 0x55,
                              0x9B, 0x0B, 0xAF, 0x66, 0x97, 0xF2, 0x08, 0x85,
                              0x01, 0x24, 0xB6, 0xED, 0x58, 0x6D, 0xE8, 0x9A};
  const std::string kKeyStr(reinterpret_cast<const char*>(kKey),
                            std::size(kKey));
  SCOPED_TRACE(testing::Message()
               << "key=" << testing::PrintToString(StringAsArray(kKeyStr)));
  // The expected result was obtained by running the Java implementation of the
  // cipher.
  const uint8_t kEncrypted[] = {
      0x02, 0x45, 0x13, 0x6A, 0xDA, 0x04, 0xEB, 0xC7, 0x6E, 0x5F, 0x6E,
      0x6F, 0x33, 0x9C, 0xC8, 0xF6, 0xE8, 0x47, 0x90, 0xC0, 0x3A, 0xC0,
      0x4C, 0xDD, 0x8B, 0xE9, 0x9C, 0x19, 0x3F, 0x25, 0x72, 0x4A, 0x58};

  std::string cipher = *CipherEncryptWithKey(kRandomString, kKeyStr);
  EXPECT_THAT(cipher,
              ElementsAreArray(reinterpret_cast<const char*>(kEncrypted),
                               std::size(kEncrypted)));
  EXPECT_THAT(CalculateECCurveHash(kRandomString),
              ElementsAreArray(*CipherDecrypt(cipher, kKeyStr)));
}

TEST(EncryptionUtils, CipherIsCommutative) {
  constexpr char kRandomString[] = "very_secret";

  // Client encrypts the string.
  std::string key_client;
  std::string cipher = *CipherEncrypt(kRandomString, &key_client);
  SCOPED_TRACE(testing::Message()
               << "key_client="
               << testing::PrintToString(StringAsArray(key_client)));

  // Server encrypts the result.
  std::string key_server;
  cipher = *CipherReEncrypt(cipher, &key_server);
  SCOPED_TRACE(testing::Message()
               << "key_server="
               << testing::PrintToString(StringAsArray(key_server)));

  EXPECT_THAT(*CipherEncryptWithKey(kRandomString, key_server),
              ElementsAreArray(*CipherDecrypt(cipher, key_client)));
}

TEST(EncryptionUtils, CreateNewKey) {
  const std::string key = *CreateNewKey();
  SCOPED_TRACE(testing::Message()
               << "key=" << testing::PrintToString(StringAsArray(key)));
  constexpr char kRandomString[] = "very_secret";

  std::string cipher = *CipherEncryptWithKey(kRandomString, key);
  EXPECT_THAT(CalculateECCurveHash(kRandomString),
              ElementsAreArray(*CipherDecrypt(cipher, key)));
}

}  // namespace password_manager
