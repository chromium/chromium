// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

namespace {

std::string RandomPassage() {
  constexpr char kLoremIpsum[] =
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
      "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
      "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea "
      "commodo consequat. Duis aute irure dolor in reprehenderit in voluptate "
      "velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint "
      "occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
      "mollit anim id est laborum.";
  static auto kLoremIpsumPieces = base::SplitStringPiece(
      kLoremIpsum, " ,.", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  constexpr size_t kWordCount = 200u;
  std::vector<std::string> pieces;
  pieces.reserve(kWordCount);
  for (size_t i = 0; i < kWordCount; i++) {
    pieces.emplace_back(
        kLoremIpsumPieces[base::RandInt(0, kLoremIpsumPieces.size() - 1)]);
  }
  return base::JoinString(pieces, " ");
}

}  // namespace

// Note: Disabled by default so as to not burden the bots. Enable when needed.
TEST(PassageCryptTest, DISABLED_EncryptDecryptMicrobenchmark) {
  OSCryptMocker::SetUp();

  constexpr size_t kPassageCount = 1000u;
  std::vector<std::string> passages;
  passages.reserve(kPassageCount);
  for (size_t i = 0; i < kPassageCount; i++) {
    passages.push_back(RandomPassage());
  }

  base::ElapsedTimer encrypt_timer;
  std::vector<std::string> encrypted;
  encrypted.reserve(kPassageCount);
  for (size_t i = 0; i < kPassageCount; i++) {
    std::string ciphertext;
    ASSERT_TRUE(OSCrypt::EncryptString(passages[i], &ciphertext));
    EXPECT_NE(passages[i], ciphertext);

    EXPECT_LT(ciphertext.size(), passages[i].size() * 2)
        << "Verify that the encryption doesn't expand the size of the original "
           "passage by more than 2x.";

    encrypted.push_back(std::move(ciphertext));
  }
  LOG(INFO) << "Encrypted " << kPassageCount << " passages in "
            << encrypt_timer.Elapsed();

  base::ElapsedTimer decrypt_timer;
  for (size_t i = 0; i < kPassageCount; i++) {
    std::string decrypted_plaintext;
    ASSERT_TRUE(OSCrypt::DecryptString(encrypted[i], &decrypted_plaintext));
    EXPECT_EQ(decrypted_plaintext, passages[i]);
  }
  LOG(INFO) << "Decrypted " << kPassageCount << " passages in "
            << decrypt_timer.Elapsed();

  OSCryptMocker::TearDown();
}

}  // namespace history_embeddings
