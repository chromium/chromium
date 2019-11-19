// Copyright 2019 The Chromium Authors. All rights reserved.
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
  auto point = group.ValueOrDie().GetPointByHashingToCurveSha256(plaintext);
  return point.ValueOrDie().ToBytesCompressed().ValueOrDie();
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
  constexpr char kExpected[] = {0x3D, 0x70, 0xD3, 0x7B, 0xFC, 0x1A, 0x3D, 0x81,
                                0x45, 0xE6, 0xC7, 0xA3, 0xA4, 0xD7, 0x92, 0x76,
                                0x61, 0xC1, 0xE8, 0xDF, 0x82, 0xBD, 0x0C, 0x9F,
                                0x61, 0x9A, 0xA3, 0xC9, 0x96, 0xEC, 0x4C, 0xB3};
  EXPECT_THAT(HashUsername("jonsnow"), ElementsAreArray(kExpected));
}

TEST(EncryptionUtils, BucketizeUsername) {
  EXPECT_THAT(BucketizeUsername("jonsnow"),
              ElementsAreArray({0x3D, 0x70, 0xD3}));
}

TEST(EncryptionUtils, ScryptHashUsernameAndPassword) {
  // The expected result was obtained by running the Java implementation of the
  // hash.
  // Needs to stay in sync with server side constant: go/passwords-leak-salts.
  constexpr char kExpected[] = {-103, 126, -10, 118,  7,    76,  -51, -76,
                                -56,  -82, -38, 31,   114,  61,  -7,  103,
                                76,   91,  52,  -52,  47,   -22, 107, 77,
                                118,  123, -14, -125, -123, 85,  115, -3};
  std::string result = ScryptHashUsernameAndPassword("user", "password123");
  EXPECT_THAT(result, ElementsAreArray(kExpected));
}

TEST(EncryptionUtils, EncryptAndDecrypt) {
  constexpr char kRandomString[] = "very_secret";
  std::string key;
  std::string cipher = CipherEncrypt(kRandomString, &key);
  SCOPED_TRACE(testing::Message()
               << "key=" << testing::PrintToString(StringAsArray(key)));

  EXPECT_NE(kRandomString, cipher);
  EXPECT_NE(std::string(), key);
  EXPECT_THAT(CalculateECCurveHash(kRandomString),
              ElementsAreArray(CipherDecrypt(cipher, key)));
}

TEST(EncryptionUtils, EncryptAndDecryptWithPredefinedKey) {
  constexpr char kRandomString[] = "very_secret";
  const std::string kKey = {-3,   -80, 44,  -113, -1,   -67, 49,  -120,
                            -91,  54,  -15, -2,   13,   -87, 95,  85,
                            -101, 11,  -81, 102,  -105, -14, 8,   -123,
                            1,    36,  -74, -19,  88,   109, -24, -102};
  SCOPED_TRACE(testing::Message()
               << "key=" << testing::PrintToString(StringAsArray(kKey)));
  // The expected result was obtained by running the Java implementation of the
  // cipher.
  const char kEncrypted[] = {2,    69,  19,  106, -38,  4,   -21,  -57, 110,
                             95,   110, 111, 51,  -100, -56, -10,  -24, 71,
                             -112, -64, 58,  -64, 76,   -35, -117, -23, -100,
                             25,   63,  37,  114, 74,   88};

  std::string cipher = CipherEncryptWithKey(kRandomString, kKey);
  EXPECT_THAT(cipher, ElementsAreArray(kEncrypted));
  EXPECT_THAT(CalculateECCurveHash(kRandomString),
              ElementsAreArray(CipherDecrypt(cipher, kKey)));
}

TEST(EncryptionUtils, CipherIsCommutative) {
  constexpr char kRandomString[] = "very_secret";

  // Client encrypts the string.
  std::string key_client;
  std::string cipher = CipherEncrypt(kRandomString, &key_client);
  SCOPED_TRACE(testing::Message()
               << "key_client="
               << testing::PrintToString(StringAsArray(key_client)));

  // Server encrypts the result.
  std::string key_server;
  cipher = CipherReEncrypt(cipher, &key_server);
  SCOPED_TRACE(testing::Message()
               << "key_server="
               << testing::PrintToString(StringAsArray(key_server)));

  EXPECT_THAT(CipherEncryptWithKey(kRandomString, key_server),
              ElementsAreArray(CipherDecrypt(cipher, key_client)));
}

}  // namespace password_manager
