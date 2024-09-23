// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/nigori/nigori.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using testing::IsNull;
using testing::NotNull;

class FakeTickClock : public base::TickClock {
 public:
  FakeTickClock() = default;

  // How much the mock clock advances after each call to the mocked
  // base::TimeTicks::Now(). We do this because we are testing functions which
  // call NowTicks() twice.
  static constexpr base::TimeDelta kTicksAdvanceAfterEachCall =
      base::Milliseconds(250);

  base::TimeTicks NowTicks() const override {
    int current_call_count = call_count_;
    ++call_count_;
    return base::TimeTicks() + current_call_count * kTicksAdvanceAfterEachCall;
  }

  int call_count() const { return call_count_; }

 private:
  mutable int call_count_ = 0;
};

constexpr base::TimeDelta FakeTickClock::kTicksAdvanceAfterEachCall;

TEST(SyncNigoriTest, GetKeyName) {
  std::unique_ptr<Nigori> nigori = Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), "password");
  ASSERT_THAT(nigori, NotNull());

  std::string expected =
      "ibGL7ymU0Si+eYCXGS6SBHPFT+JCYiB6GDOYqj6vIwEi"
      "WJ7RENSHxmIQ8Q3rXd/UnZUmFHYB+jSIbthQADXvrQ==";
  EXPECT_EQ(expected, nigori->GetKeyName());
}

TEST(SyncNigoriTest, GetKeyNameIsConstant) {
  std::unique_ptr<Nigori> nigori1 = Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), "password");
  ASSERT_THAT(nigori1, NotNull());

  std::string keyname1 = nigori1->GetKeyName();

  std::unique_ptr<Nigori> nigori2 = Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), "password");
  ASSERT_THAT(nigori2, NotNull());

  std::string keyname2 = nigori2->GetKeyName();

  EXPECT_LT(0U, keyname1.size());
  EXPECT_EQ(keyname1, keyname2);
}

TEST(SyncNigoriTest, EncryptDifferentIv) {
  std::unique_ptr<Nigori> nigori = Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), "password");
  ASSERT_THAT(nigori, NotNull());

  const std::string plaintext("value");
  EXPECT_NE(nigori->Encrypt(plaintext), nigori->Encrypt(plaintext));
}

TEST(SyncNigoriTest, Decrypt) {
  std::unique_ptr<Nigori> nigori = Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), "password");
  ASSERT_THAT(nigori, NotNull());

  std::string encrypted =
      "NNYlnzaaLPXWXyzz8J+u4OKgLiKRBPu2GJdjHWk0m3ADZrJhnmer30"
      "Zgiy4Ulxlfh6fmS71k8rop+UvSJdL1k/fcNLJ1C6sY5Z86ijyl1Jo=";

  std::string plaintext;
  EXPECT_TRUE(nigori->Decrypt(encrypted, &plaintext));

  std::string expected("test, test, 1, 2, 3");
  EXPECT_EQ(expected, plaintext);
}

TEST(SyncNigoriTest, EncryptDecrypt) {
  std::unique_ptr<Nigori> nigori = Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), "password");
  ASSERT_THAT(nigori, NotNull());

  const std::string plaintext("value");

  std::string decrypted;
  EXPECT_TRUE(nigori->Decrypt(nigori->Encrypt(plaintext), &decrypted));

  EXPECT_EQ(plaintext, decrypted);
}

TEST(SyncNigoriTest, EncryptDecryptEmptyString) {
  std::unique_ptr<Nigori> nigori = Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), "password");
  ASSERT_THAT(nigori, NotNull());

  const std::string plaintext;

  std::string decrypted;
  EXPECT_TRUE(nigori->Decrypt(nigori->Encrypt(plaintext), &decrypted));

  EXPECT_EQ(plaintext, decrypted);
}

TEST(SyncNigoriTest, CorruptedIv) {
  std::unique_ptr<Nigori> nigori = Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), "password");
  ASSERT_THAT(nigori, NotNull());

  const std::string plaintext("test");

  std::string encrypted = nigori->Encrypt(plaintext);

  // Corrupt the IV by changing one of its byte.
  encrypted[0] = (encrypted[0] == 'a' ? 'b' : 'a');

  std::string decrypted;
  EXPECT_TRUE(nigori->Decrypt(encrypted, &decrypted));

  EXPECT_NE(plaintext, decrypted);
}

TEST(SyncNigoriTest, CorruptedCiphertext) {
  std::unique_ptr<Nigori> nigori = Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), "password");
  ASSERT_THAT(nigori, NotNull());

  const std::string plaintext("test");

  std::string encrypted = nigori->Encrypt(plaintext);

  // Corrput the ciphertext by changing one of its bytes.
  encrypted[Nigori::kIvSize + 10] =
      (encrypted[Nigori::kIvSize + 10] == 'a' ? 'b' : 'a');

  std::string decrypted;
  EXPECT_FALSE(nigori->Decrypt(encrypted, &decrypted));

  EXPECT_NE(plaintext, decrypted);
}

TEST(SyncNigoriTest, ExportImport) {
  std::unique_ptr<Nigori> nigori1 = Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), "password");
  ASSERT_THAT(nigori1, NotNull());

  std::string user_key;
  std::string encryption_key;
  std::string mac_key;
  nigori1->ExportKeys(&user_key, &encryption_key, &mac_key);

  std::unique_ptr<Nigori> nigori2 =
      Nigori::CreateByImport(user_key, encryption_key, mac_key);
  ASSERT_THAT(nigori2, NotNull());

  std::string original("test");
  std::string plaintext;
  std::string ciphertext;

  EXPECT_TRUE(nigori2->Decrypt(nigori1->Encrypt(original), &plaintext));
  EXPECT_EQ(original, plaintext);

  EXPECT_TRUE(nigori1->Decrypt(nigori2->Encrypt(original), &plaintext));
  EXPECT_EQ(original, plaintext);

  std::string keyname1 = nigori1->GetKeyName();
  EXPECT_FALSE(keyname1.empty());
  EXPECT_EQ(keyname1, nigori2->GetKeyName());
}

TEST(SyncNigoriTest, CreateByDerivationSetsUserKey) {
  std::unique_ptr<Nigori> nigori = Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), "password");
  ASSERT_THAT(nigori, NotNull());

  std::string user_key;
  std::string encryption_key;
  std::string mac_key;
  nigori->ExportKeys(&user_key, &encryption_key, &mac_key);

  EXPECT_NE(user_key, "");
}

TEST(SyncNigoriTest, ToleratesEmptyUserKey) {
  std::unique_ptr<Nigori> nigori1 = Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), "password");
  ASSERT_THAT(nigori1, NotNull());

  std::string user_key;
  std::string encryption_key;
  std::string mac_key;
  nigori1->ExportKeys(&user_key, &encryption_key, &mac_key);
  EXPECT_FALSE(user_key.empty());
  EXPECT_FALSE(encryption_key.empty());
  EXPECT_FALSE(mac_key.empty());

  std::unique_ptr<Nigori> nigori2 =
      Nigori::CreateByImport(/*user_key=*/"", encryption_key, mac_key);
  ASSERT_THAT(nigori2, NotNull());

  user_key = "non-empty-value";
  nigori2->ExportKeys(&user_key, &encryption_key, &mac_key);
  EXPECT_TRUE(user_key.empty());
  EXPECT_FALSE(encryption_key.empty());
  EXPECT_FALSE(mac_key.empty());
}

TEST(SyncNigoriTest, CreateByDerivationShouldDeriveCorrectKeyUsingPbkdf2) {
  const std::string kPassphrase = "hunter2";

  std::unique_ptr<Nigori> nigori = Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForPbkdf2(), kPassphrase);
  ASSERT_THAT(nigori, NotNull());

  std::string user_key;
  std::string encryption_key;
  std::string mac_key;
  nigori->ExportKeys(&user_key, &encryption_key, &mac_key);
  // These are reference values obtained by running PBKDF2 with Nigori's
  // parameters and the input values given above.
  EXPECT_EQ("025599e143c4923d77f65b99d97019a3",
            base::ToLowerASCII(base::HexEncode(user_key)));
  EXPECT_EQ("4596bf346572497d92b2a0e2146d93c1",
            base::ToLowerASCII(base::HexEncode(encryption_key)));
  EXPECT_EQ("2292ad9db96fe590b22a58db50f6f545",
            base::ToLowerASCII(base::HexEncode(mac_key)));
}

TEST(SyncNigoriTest, CreateByDerivationShouldDeriveCorrectKeyUsingScrypt) {
  const std::string kSalt = "alpensalz";
  const std::string kPassphrase = "hunter2";

  std::unique_ptr<Nigori> nigori = Nigori::CreateByDerivation(
      KeyDerivationParams::CreateForScrypt(kSalt), kPassphrase);
  ASSERT_THAT(nigori, NotNull());

  std::string user_key;
  std::string encryption_key;
  std::string mac_key;
  nigori->ExportKeys(&user_key, &encryption_key, &mac_key);
  // user_key is not used anymore, but is being set for backwards compatibility
  // (because legacy clients cannot import a Nigori node without one).
  // Therefore, we just initialize it to all zeroes.
  EXPECT_EQ("00000000000000000000000000000000",
            base::ToLowerASCII(base::HexEncode(user_key)));
  // These are reference values obtained by running scrypt with Nigori's
  // parameters and the input values given above.
  EXPECT_EQ("8aa735e0091339a5e51da3b3dd1b328a",
            base::ToLowerASCII(base::HexEncode(encryption_key)));
  EXPECT_EQ("a7e73611968dfd2bca5b3382aed451ba",
            base::ToLowerASCII(base::HexEncode(mac_key)));
}

TEST(SyncNigoriTest, CreateByDerivationShouldReportPbkdf2DurationInHistogram) {
  FakeTickClock fake_tick_clock;
  base::HistogramTester histogram_tester;

  std::unique_ptr<Nigori> nigori = Nigori::CreateByDerivationForTesting(
      KeyDerivationParams::CreateForPbkdf2(), "Passphrase!", &fake_tick_clock);
  ASSERT_THAT(nigori, NotNull());
  ASSERT_EQ(2, fake_tick_clock.call_count());

  histogram_tester.ExpectUniqueSample(
      "Sync.Crypto.NigoriKeyDerivationDuration.Pbkdf2",
      /*sample=*/FakeTickClock::kTicksAdvanceAfterEachCall.InMilliseconds(),
      /*expected_bucket_count=*/1);
}

TEST(SyncNigoriTest, CreateByDerivationShouldReportScryptDurationInHistogram) {
  Nigori::SetUseScryptCostParameterForTesting(true);

  FakeTickClock fake_tick_clock;
  base::HistogramTester histogram_tester;

  std::unique_ptr<Nigori> nigori = Nigori::CreateByDerivationForTesting(
      KeyDerivationParams::CreateForScrypt("somesalt"), "Passphrase!",
      &fake_tick_clock);
  ASSERT_THAT(nigori, NotNull());
  ASSERT_EQ(2, fake_tick_clock.call_count());

  histogram_tester.ExpectUniqueSample(
      "Sync.Crypto.NigoriKeyDerivationDuration.Scrypt8192",
      /*sample=*/FakeTickClock::kTicksAdvanceAfterEachCall.InMilliseconds(),
      /*expected_bucket_count=*/1);

  Nigori::SetUseScryptCostParameterForTesting(false);
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

TEST(SyncNigoriTest, ShouldFailToImportEmpty) {
  EXPECT_THAT(Nigori::CreateByImport("", "", ""), IsNull());
}

TEST(SyncNigoriTest, ShouldFailToImportInvalid) {
  EXPECT_THAT(Nigori::CreateByImport("foo", "bar", "baz"), IsNull());
}

}  // anonymous namespace
}  // namespace syncer
