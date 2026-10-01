// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/weak_check_utility.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/password_manager/core/browser/password_string.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace password_manager {

namespace {

constexpr char16_t kWeakShortPassword[] = u"123456";
constexpr char16_t kWeakLongPassword[] =
    u"abcdabcdabcdabcdabcdabcdabcdabcdabcdabcda";
constexpr char16_t kStrongShortPassword[] = u"fnlsr4@cm^mdls@fkspnsg3d";
constexpr char16_t kStrongLongPassword[] =
    u"pmsFlsnoab4nsl#losb@skpfnsbkjb^klsnbs!cns";
constexpr char16_t kLongPasswordWithEmoji[] =
    u"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\U0001F995";

using ::testing::UnorderedElementsAre;

}  // namespace

TEST(WeakCheckUtilityTest, IsWeak) {
  EXPECT_TRUE(IsWeak(PasswordString(kWeakShortPassword)));
  EXPECT_TRUE(IsWeak(PasswordString(kWeakLongPassword)));
  EXPECT_FALSE(IsWeak(PasswordString(kStrongShortPassword)));
  EXPECT_FALSE(IsWeak(PasswordString(kStrongLongPassword)));
}

TEST(WeakCheckUtilityTest, IsWeakPlaintext) {
  EXPECT_TRUE(IsWeak(std::u16string(kWeakShortPassword)));
  EXPECT_TRUE(IsWeak(std::u16string(kWeakLongPassword)));
  EXPECT_FALSE(IsWeak(std::u16string(kStrongShortPassword)));
  EXPECT_FALSE(IsWeak(std::u16string(kStrongLongPassword)));
  EXPECT_TRUE(IsWeak(std::u16string(kLongPasswordWithEmoji)));
}

TEST(WeakCheckUtilityTest, IsWeakRecordsMetrics) {
  base::HistogramTester histogram_tester;

  EXPECT_TRUE(IsWeak(PasswordString(kWeakLongPassword)));
  EXPECT_FALSE(IsWeak(PasswordString(kStrongShortPassword)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples("PasswordManager.WeakCheck.PasswordScore"),
      base::BucketsAre(base::Bucket(0, 1), base::Bucket(4, 1)));
}

TEST(WeakCheckUtilityTest, WeakPasswordsNotFound) {
  absl::flat_hash_set<PasswordString> passwords = {
      PasswordString(kStrongShortPassword),
      PasswordString(kStrongLongPassword)};

  EXPECT_THAT(BulkWeakCheck(passwords), testing::IsEmpty());
}

TEST(WeakCheckUtilityTest, DetectedShortAndLongWeakPasswords) {
  absl::flat_hash_set<PasswordString> passwords = {
      PasswordString(kStrongLongPassword), PasswordString(kWeakShortPassword),
      PasswordString(kStrongShortPassword), PasswordString(kWeakLongPassword)};

  absl::flat_hash_set<PasswordString> weak_passwords = BulkWeakCheck(passwords);

  EXPECT_THAT(weak_passwords,
              UnorderedElementsAre(kWeakShortPassword, kWeakLongPassword));
}

TEST(WeakCheckUtilityTest, HandlesUTF16SurrogatePairs) {
  // Password with dinosaur emojis: pass🦕word🦖123
  // Consists of: pass + 🦕 + word + 🦖 + 123
  const char16_t kPasswordWithEmojis[] = u"pass\U0001F995word\U0001F996123";

  // Long with complex emoji: aaaaa...👨‍👩‍👧‍👦
  // Consists of: 37 'a' characters + 👨 + ZWJ + 👩 + ZWJ + 👧 + ZWJ + 👦
  const char16_t kLongPasswordWithComplexEmoji[] =
      u"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\U0001F468\u200D\U0001F469\u200D"
      u"\U0001F467\u200D\U0001F466";

  // Complex emoji only:
  // 👨‍👩‍👧‍👦🤹‍♂️👩‍🔬👨‍👨‍👧‍👧
  // Consists of: (👨+ZWJ+👩+ZWJ+👧+ZWJ+👦) + (🤹+ZWJ+♂️) + (👩+ZWJ+🔬) +
  // (👨+ZWJ+👨+ZWJ+👧+ZWJ+👧)
  const char16_t kComplexEmojiPassword[] =
      u"\U0001F468\u200D\U0001F469\u200D\U0001F467\u200D\U0001F466"
      u"\U0001F939\u200D\u2642\uFE0F"
      u"\U0001F469\u200D\U0001F52C"
      u"\U0001F468\u200D\U0001F468\u200D\U0001F467\u200D\U0001F467";

  EXPECT_FALSE(static_cast<bool>(IsWeak(PasswordString(kPasswordWithEmojis))));

  IsWeakPassword is_weak = IsWeak(PasswordString(kLongPasswordWithEmoji));

  EXPECT_TRUE(static_cast<bool>(is_weak));

  absl::flat_hash_set<PasswordString> passwords = {
      PasswordString(kPasswordWithEmojis),
      PasswordString(kLongPasswordWithEmoji),
      PasswordString(kLongPasswordWithComplexEmoji),
      PasswordString(kComplexEmojiPassword)};

  absl::flat_hash_set<PasswordString> weak_passwords = BulkWeakCheck(passwords);

  EXPECT_THAT(weak_passwords, UnorderedElementsAre(kLongPasswordWithEmoji));
}

TEST(WeakCheckUtilityTest, SafeTruncateUTF16HandlesEmojis) {
  // Family emoji: 👨‍👩‍👧‍👦
  // Consists of: 👨 + ZWJ + 👩 + ZWJ + 👧 + ZWJ + 👦
  const char16_t kFamilyEmoji[] =
      u"123\U0001F468\u200D\U0001F469\u200D\U0001F467\u200D\U0001F466789";
  EXPECT_EQ(SafeTruncateUTF16(kFamilyEmoji, 4),
            u"123\U0001F468\u200D\U0001F469\u200D\U0001F467\u200D\U0001F466");

  // Key with sparkles emoji: 🔑✨
  // Consists of: 🔑 + ZWJ + ✨
  const char16_t kKeyWithSparkles[] = u"123\U0001F511\u200D\u2728789";

  EXPECT_EQ(SafeTruncateUTF16(kKeyWithSparkles, 4),
            u"123\U0001F511\u200D\u2728");

  // Family emoji + T-Rex emoji: 👨‍👩‍👧‍👦🦕
  // Consists of: 👨 + ZWJ + 👩 + ZWJ + 👧 + ZWJ + 👦 + 🦕
  const char16_t kMixedEmojis[] =
      u"12\U0001F468\u200D\U0001F469\u200D\U0001F467\u200D\U0001F466\U0001F995";
  EXPECT_EQ(SafeTruncateUTF16(kMixedEmojis, 3),
            u"12\U0001F468\u200D\U0001F469\u200D\U0001F467\u200D\U0001F466");
}

TEST(SafeTruncateUTF16Test, HandlesEdgeCases) {
  // Test empty string
  EXPECT_EQ(SafeTruncateUTF16(u"", 5), u"");

  // Test max_length == 0
  EXPECT_EQ(SafeTruncateUTF16(u"Hello", 0), u"");
  EXPECT_EQ(SafeTruncateUTF16(u"👋", 0), u"");
}

}  // namespace password_manager
