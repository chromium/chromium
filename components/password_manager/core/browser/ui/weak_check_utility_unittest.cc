// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/weak_check_utility.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

constexpr char16_t kWeakShortPassword[] = u"123456";
constexpr char16_t kWeakLongPassword[] =
    u"abcdabcdabcdabcdabcdabcdabcdabcdabcdabcda";
constexpr char16_t kStrongShortPassword[] = u"fnlsr4@cm^mdls@fkspnsg3d";
constexpr char16_t kStrongLongPassword[] =
    u"pmsFlsnoab4nsl#losb@skpfnsbkjb^klsnbs!cns";

using ::testing::ElementsAre;

}  // namespace

TEST(WeakCheckUtilityTest, IsWeak) {
  EXPECT_TRUE(IsWeak(kWeakShortPassword));
  EXPECT_TRUE(IsWeak(kWeakLongPassword));
  EXPECT_FALSE(IsWeak(kStrongShortPassword));
  EXPECT_FALSE(IsWeak(kStrongLongPassword));
}

TEST(WeakCheckUtilityTest, IsWeakRecordsMetrics) {
  base::HistogramTester histogram_tester;

  EXPECT_TRUE(IsWeak(kWeakLongPassword));
  EXPECT_FALSE(IsWeak(kStrongShortPassword));

  EXPECT_THAT(
      histogram_tester.GetAllSamples("PasswordManager.WeakCheck.PasswordScore"),
      base::BucketsAre(base::Bucket(0, 1), base::Bucket(4, 1)));
}

TEST(WeakCheckUtilityTest, WeakPasswordsNotFound) {
  base::flat_set<std::u16string> passwords = {kStrongShortPassword,
                                              kStrongLongPassword};

  EXPECT_THAT(BulkWeakCheck(passwords), testing::IsEmpty());
}

TEST(WeakCheckUtilityTest, DetectedShortAndLongWeakPasswords) {
  base::flat_set<std::u16string> passwords = {
      kStrongLongPassword, kWeakShortPassword, kStrongShortPassword,
      kWeakLongPassword};

  base::flat_set<std::u16string> weak_passwords = BulkWeakCheck(passwords);

  EXPECT_THAT(weak_passwords,
              ElementsAre(kWeakShortPassword, kWeakLongPassword));
}

TEST(WeakCheckUtilityTest, HandlesUTF16SurrogatePairs) {
  // Password with dinosaur emojis: pass🦕word🦖123
  // Consists of: pass + 🦕 + word + 🦖 + 123
  const char16_t kPasswordWithEmojis[] = u"pass\U0001F995word\U0001F996123";

  // Long password with emoji: aaaaa...🦕
  // Consists of: 37 'a' characters + 🦕
  const char16_t kLongPasswordWithEmoji[] =
      u"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\U0001F995";

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

  EXPECT_FALSE(static_cast<bool>(IsWeak(kPasswordWithEmojis)));

  IsWeakPassword is_weak = IsWeak(kLongPasswordWithEmoji);

  EXPECT_TRUE(static_cast<bool>(is_weak));

  base::flat_set<std::u16string> passwords = {
      kPasswordWithEmojis, kLongPasswordWithEmoji,
      kLongPasswordWithComplexEmoji, kComplexEmojiPassword};

  base::flat_set<std::u16string> weak_passwords = BulkWeakCheck(passwords);

  EXPECT_THAT(weak_passwords, ElementsAre(kLongPasswordWithEmoji));
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
