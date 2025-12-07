// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/translation_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace captions {
class TranslationUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    translation_cache_ = std::make_unique<TranslationCache>();
  }
  std::unique_ptr<TranslationCache> translation_cache_;
};

TEST_F(TranslationUtilTest, CanSplitSentences) {
  std::string text = "Hello. How are you? I am fine. Thank you.";
  std::string locale = "en_US";
  std::vector<std::string> sentences = SplitSentences(text, locale);
  EXPECT_EQ(sentences.size(), 4u);
  EXPECT_EQ(sentences[0], "Hello. ");
  EXPECT_EQ(sentences[1], "How are you? ");
  EXPECT_EQ(sentences[2], "I am fine. ");
  EXPECT_EQ(sentences[3], "Thank you.");
}

TEST_F(TranslationUtilTest, NoticesTrailingSpace) {
  std::string with_trailing_space = "Hello. ";
  std::string without_trailing_space = "Hello.";
  EXPECT_TRUE(ContainsTrailingSpace(with_trailing_space));
  EXPECT_FALSE(ContainsTrailingSpace(without_trailing_space));
}

TEST_F(TranslationUtilTest, RemovesTrailingSpace) {
  std::string result = RemoveTrailingSpace("Hello. ");
  EXPECT_EQ(result, "Hello.");
}

TEST_F(TranslationUtilTest, CanGetTranslationCacheKey) {
  std::string source_language = "en";
  std::string target_language = "es";
  std::string transcription = "Hello. How are you? I am fine. Thank you.";
  std::string cache_key =
      GetTranslationCacheKey(source_language, target_language, transcription);
  EXPECT_EQ(cache_key, "enes|hello how are you i am fine thank you");
}

TEST_F(TranslationUtilTest, IdentifiesIdeographicLocale) {
  EXPECT_TRUE(IsIdeographicLocale("ja_JP"));
  EXPECT_TRUE(IsIdeographicLocale("ja"));
  EXPECT_TRUE(IsIdeographicLocale("ja-JP"));
  EXPECT_FALSE(IsIdeographicLocale("en_US"));
  EXPECT_FALSE(IsIdeographicLocale("en"));
  EXPECT_FALSE(IsIdeographicLocale("en-US"));
}

TEST_F(TranslationUtilTest, ReturnsCachedTranslation) {
  translation_cache_->InsertIntoCache(
      "Hello. How are you? I am fine. Thank you.",
      "Hola. Como estas? Estoy bien. Gracias.", "en_US", "es_ES");
  auto [remaining, cached] =
      translation_cache_->FindCachedTranslationOrRemaining(
          "Hello. How are you? I am fine. Thank you.", "en_US", "es_ES");
  EXPECT_EQ(remaining, "Thank you.");
  EXPECT_EQ(cached, "Hola. Como estas? Estoy bien. ");
  translation_cache_->Clear();
}
TEST_F(TranslationUtilTest, ReturnsRemainingTranslation) {
  auto [remaining, cached] =
      translation_cache_->FindCachedTranslationOrRemaining(
          "Hello. How are you? I am fine. Thank you.", "en_US", "es_ES");
  EXPECT_EQ(remaining, "Hello. How are you? I am fine. Thank you.");
  EXPECT_EQ(cached, "");
  translation_cache_->Clear();
}

}  // namespace captions
