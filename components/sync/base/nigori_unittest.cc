// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/nigori.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/tick_clock.h"
#include "components/sync/base/sync_base_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class FakeTickClock : public base::TickClock {
 public:
  FakeTickClock() : call_count_(0) {}

  // How much the mock clock advances after each call to the mocked
  // base::TimeTicks::Now(). We do this because we are testing functions which
  // call NowTicks() twice.
  static constexpr base::TimeDelta kTicksAdvanceAfterEachCall =
      base::TimeDelta::FromMilliseconds(250);

  base::TimeTicks NowTicks() const override {
    int current_call_count = call_count_;
    ++call_count_;
    return base::TimeTicks() + current_call_count * kTicksAdvanceAfterEachCall;
  }

  int call_count() const { return call_count_; }

 private:
  mutable int call_count_;
};

constexpr base::TimeDelta FakeTickClock::kTicksAdvanceAfterEachCall;

TEST(SyncNigoriTest, Permute) {
  Nigori nigori;
  EXPECT_TRUE(nigori.InitByDerivation(KeyDerivationParams::CreateForPbkdf2(),
                                      "password"));

  std::string permuted;
  EXPECT_TRUE(nigori.Permute(Nigori::Password, "test name", &permuted));

  std::string expected =
      "evpwwPT726JS/mhxv1UwPVLDz5ha/GrMA8HfA3sQGJr5"
      "5zFtrFep7DXu9FIyGbBEdlHNWtwVlPVE5FEgyoV++w==";
  EXPECT_EQ(expected, permuted);
}

TEST(SyncNigoriTest, PermuteIsConstant) {
  Nigori nigori1;
  EXPECT_TRUE(nigori1.InitByDerivation(KeyDerivationParams::CreateForPbkdf2(),
                                       "password"));

  std::string permuted1;
  EXPECT_TRUE(nigori1.Permute(Nigori::Password, "name", &permuted1));

  Nigori nigori2;
  EXPECT_TRUE(nigori2.InitByDerivation(KeyDerivationParams::CreateForPbkdf2(),
                                       "password"));

  std::string permuted2;
  EXPECT_TRUE(nigori2.Permute(Nigori::Password, "name", &permuted2));

  EXPECT_LT(0U, permuted1.size());
  EXPECT_EQ(permuted1, permuted2);
}

TEST(SyncNigoriTest, EncryptDifferentIv) {
  Nigori nigori;
  EXPECT_TRUE(nigori.InitByDerivation(KeyDerivationParams::CreateForPbkdf2(),
                                      "password"));

  std::string plaintext("value");

  std::string encrypted1;
  EXPECT_TRUE(nigori.Encrypt(plaintext, &encrypted1));

  std::string encrypted2;
  EXPECT_TRUE(nigori.Encrypt(plaintext, &encrypted2));

  EXPECT_NE(encrypted1, encrypted2);
}

TEST(SyncNigoriTest, Decrypt) {
  Nigori nigori;
  EXPECT_TRUE(nigori.InitByDerivation(KeyDerivationParams::CreateForPbkdf2(),
                                      "password"));

  std::string encrypted =
      "NNYlnzaaLPXWXyzz8J+u4OKgLiKRBPu2GJdjHWk0m3ADZrJhnmer30"
      "Zgiy4Ulxlfh6fmS71k8rop+UvSJdL1k/fcNLJ1C6sY5Z86ijyl1Jo=";

  std::string plaintext;
  EXPECT_TRUE(nigori.Decrypt(encrypted, &plaintext));

  std::string expected("test, test, 1, 2, 3");
  EXPECT_EQ(expected, plaintext);
}

TEST(SyncNigoriTest, EncryptDecrypt) {
  Nigori nigori;
  EXPECT_TRUE(nigori.InitByDerivation(KeyDerivationParams::CreateForPbkdf2(),
                                      "password"));

  std::string plaintext("value");

  std::string encrypted;
  EXPECT_TRUE(nigori.Encrypt(plaintext, &encrypted));

  std::string decrypted;
  EXPECT_TRUE(nigori.Decrypt(encrypted, &decrypted));

  EXPECT_EQ(plaintext, decrypted);
}

TEST(SyncNigoriTest, CorruptedIv) {
  Nigori nigori;
  EXPECT_TRUE(nigori.InitByDerivation(KeyDerivationParams::CreateForPbkdf2(),
                                      "password"));

  std::string plaintext("test");

  std::string encrypted;
  EXPECT_TRUE(nigori.Encrypt(plaintext, &encrypted));

  // Corrupt the IV by changing one of its byte.
  encrypted[0] = (encrypted[0] == 'a' ? 'b' : 'a');

  std::string decrypted;
  EXPECT_TRUE(nigori.Decrypt(encrypted, &decrypted));

  EXPECT_NE(plaintext, decrypted);
}

TEST(SyncNigoriTest, CorruptedCiphertext) {
  Nigori nigori;
  EXPECT_TRUE(nigori.InitByDerivation(KeyDerivationParams::CreateForPbkdf2(),
                                      "password"));

  std::string plaintext("test");

  std::string encrypted;
  EXPECT_TRUE(nigori.Encrypt(plaintext, &encrypted));

  // Corrput the ciphertext by changing one of its bytes.
  encrypted[Nigori::kIvSize + 10] =
      (encrypted[Nigori::kIvSize + 10] == 'a' ? 'b' : 'a');

  std::string decrypted;
  EXPECT_FALSE(nigori.Decrypt(encrypted, &decrypted));

  EXPECT_NE(plaintext, decrypted);
}

TEST(SyncNigoriTest, ExportImport) {
  Nigori nigori1;
  EXPECT_TRUE(nigori1.InitByDerivation(KeyDerivationParams::CreateForPbkdf2(),
                                       "password"));

  std::string user_key;
  std::string encryption_key;
  std::string mac_key;
  nigori1.ExportKeys(&user_key, &encryption_key, &mac_key);

  Nigori nigori2;
  EXPECT_TRUE(nigori2.InitByImport(user_key, encryption_key, mac_key));

  std::string original("test");
  std::string plaintext;
  std::string ciphertext;

  EXPECT_TRUE(nigori1.Encrypt(original, &ciphertext));
  EXPECT_TRUE(nigori2.Decrypt(ciphertext, &plaintext));
  EXPECT_EQ(original, plaintext);

  EXPECT_TRUE(nigori2.Encrypt(original, &ciphertext));
  EXPECT_TRUE(nigori1.Decrypt(ciphertext, &plaintext));
  EXPECT_EQ(original, plaintext);

  std::string permuted1, permuted2;
  EXPECT_TRUE(nigori1.Permute(Nigori::Password, original, &permuted1));
  EXPECT_TRUE(nigori2.Permute(Nigori::Password, original, &permuted2));
  EXPECT_EQ(permuted1, permuted2);
}

TEST(SyncNigoriTest, InitByDerivationSetsUserKey) {
  Nigori nigori;
  EXPECT_TRUE(nigori.InitByDerivation(KeyDerivationParams::CreateForPbkdf2(),
                                      "password"));

  std::string user_key;
  std::string encryption_key;
  std::string mac_key;
  nigori.ExportKeys(&user_key, &encryption_key, &mac_key);

  EXPECT_NE(user_key, "");
}

TEST(SyncNigoriTest, ToleratesEmptyUserKey) {
  Nigori nigori1;
  EXPECT_TRUE(nigori1.InitByDerivation(KeyDerivationParams::CreateForPbkdf2(),
                                       "password"));

  std::string user_key;
  std::string encryption_key;
  std::string mac_key;
  nigori1.ExportKeys(&user_key, &encryption_key, &mac_key);
  EXPECT_FALSE(user_key.empty());
  EXPECT_FALSE(encryption_key.empty());
  EXPECT_FALSE(mac_key.empty());

  Nigori nigori2;
  EXPECT_TRUE(nigori2.InitByImport("", encryption_key, mac_key));

  user_key = "non-empty-value";
  nigori2.ExportKeys(&user_key, &encryption_key, &mac_key);
  EXPECT_TRUE(user_key.empty());
  EXPECT_FALSE(encryption_key.empty());
  EXPECT_FALSE(mac_key.empty());
}

TEST(SyncNigoriTest, InitByDerivationShouldDeriveCorrectKeyUsingPbkdf2) {
  const std::string kPassphrase = "hunter2";

  Nigori nigori;
  EXPECT_TRUE(nigori.InitByDerivation(KeyDerivationParams::CreateForPbkdf2(),
                                      kPassphrase));

  std::string user_key;
  std::string encryption_key;
  std::string mac_key;
  nigori.ExportKeys(&user_key, &encryption_key, &mac_key);
  // These are reference values obtained by running PBKDF2 with Nigori's
  // parameters and the input values given above.
  EXPECT_EQ(
      "025599e143c4923d77f65b99d97019a3",
      base::ToLowerASCII(base::HexEncode(user_key.data(), user_key.size())));
  EXPECT_EQ("4596bf346572497d92b2a0e2146d93c1",
            base::ToLowerASCII(
                base::HexEncode(encryption_key.data(), encryption_key.size())));
  EXPECT_EQ(
      "2292ad9db96fe590b22a58db50f6f545",
      base::ToLowerASCII(base::HexEncode(mac_key.data(), mac_key.size())));
}

TEST(SyncNigoriTest, InitByDerivationShouldDeriveCorrectKeyUsingScrypt) {
  const std::string kSalt = "alpensalz";
  const std::string kPassphrase = "hunter2";

  Nigori nigori;
  EXPECT_TRUE(nigori.InitByDerivation(
      KeyDerivationParams::CreateForScrypt(kSalt), kPassphrase));

  std::string user_key;
  std::string encryption_key;
  std::string mac_key;
  nigori.ExportKeys(&user_key, &encryption_key, &mac_key);
  // user_key is not used anymore, but is being set for backwards compatibility
  // (because legacy clients cannot import a Nigori node without one).
  // Therefore, we just initialize it to all zeroes.
  EXPECT_EQ(
      "00000000000000000000000000000000",
      base::ToLowerASCII(base::HexEncode(user_key.data(), user_key.size())));
  // These are reference values obtained by running scrypt with Nigori's
  // parameters and the input values given above.
  EXPECT_EQ("8aa735e0091339a5e51da3b3dd1b328a",
            base::ToLowerASCII(
                base::HexEncode(encryption_key.data(), encryption_key.size())));
  EXPECT_EQ(
      "a7e73611968dfd2bca5b3382aed451ba",
      base::ToLowerASCII(base::HexEncode(mac_key.data(), mac_key.size())));
}

TEST(SyncNigoriTest,
     InitByDerivationShouldFailWhenGivenUnsupportedKeyDerivationMethod) {
  Nigori nigori;
  EXPECT_FALSE(nigori.InitByDerivation(
      KeyDerivationParams::CreateWithUnsupportedMethod(), "Passphrase!"));
}

TEST(SyncNigoriTest, InitByDerivationShouldReportPbkdf2DurationInHistogram) {
  FakeTickClock fake_tick_clock;
  Nigori nigori;
  nigori.SetTickClockForTesting(&fake_tick_clock);
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(nigori.InitByDerivation(KeyDerivationParams::CreateForPbkdf2(),
                                      "Passphrase!"));
  ASSERT_EQ(2, fake_tick_clock.call_count());

  histogram_tester.ExpectUniqueSample(
      "Sync.Crypto.NigoriKeyDerivationDuration.Pbkdf2",
      /*sample=*/FakeTickClock::kTicksAdvanceAfterEachCall.InMilliseconds(),
      /*count=*/1);
}

TEST(SyncNigoriTest, InitByDerivationShouldReportScryptDurationInHistogram) {
  FakeTickClock fake_tick_clock;
  Nigori nigori;
  nigori.SetTickClockForTesting(&fake_tick_clock);
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(nigori.InitByDerivation(
      KeyDerivationParams::CreateForScrypt("somesalt"), "Passphrase!"));
  ASSERT_EQ(2, fake_tick_clock.call_count());

  histogram_tester.ExpectUniqueSample(
      "Sync.Crypto.NigoriKeyDerivationDuration.Scrypt8192",
      /*sample=*/FakeTickClock::kTicksAdvanceAfterEachCall.InMilliseconds(),
      /*count=*/1);
}

TEST(SyncNigoriTest, GenerateScryptSaltShouldReturnSaltOfCorrectSize) {
  EXPECT_EQ(32U, Nigori::GenerateScryptSalt().size());
}

TEST(SyncNigoriTest, GenerateScryptSaltShouldReturnNontrivialSalt) {
  std::string salt = Nigori::GenerateScryptSalt();
  // Check that there are at least two different bytes in the salt. Guards
  // against e.g. a salt of all zeroes.
  bool is_nontrivial = false;
  for (char c : salt) {
    if (c != salt[0]) {
      is_nontrivial = true;
      break;
    }
  }
  EXPECT_TRUE(is_nontrivial);
}

}  // anonymous namespace
}  // namespace syncer
