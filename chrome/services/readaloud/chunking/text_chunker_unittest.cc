// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/chunking/text_chunker.h"

#include <string>
#include <vector>

#include "base/i18n/language_tag.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace readaloud {

using base::i18n::GetKnownLanguageTag;

using TextChunkerTest = testing::Test;

TEST_F(TextChunkerTest, EmptyTextReturnsEmptyChunks) {
  std::vector<TextChunk> chunks = ChunkText(u"", ChunkingMode::kSpeed);
  EXPECT_TRUE(chunks.empty());
}

TEST_F(TextChunkerTest, SingleSentenceSpeedMode) {
  std::u16string text = u"Hello world, this is a single sentence.";
  std::vector<TextChunk> chunks = ChunkText(text, ChunkingMode::kSpeed);

  ASSERT_EQ(chunks.size(), 1u);
  EXPECT_EQ(chunks[0].text, u"Hello world, this is a single sentence.");
  EXPECT_EQ(chunks[0].start_code_unit_offset, 0u);
}

TEST_F(TextChunkerTest, MultiSentenceSpeedMode) {
  std::u16string text = u"First sentence. Second sentence! Third sentence?";
  std::vector<TextChunk> chunks = ChunkText(text, ChunkingMode::kSpeed);

  ASSERT_EQ(chunks.size(), 3u);
  EXPECT_EQ(chunks[0].text, u"First sentence.");
  EXPECT_EQ(chunks[0].start_code_unit_offset, 0u);

  EXPECT_EQ(chunks[1].text, u"Second sentence!");
  EXPECT_EQ(chunks[1].start_code_unit_offset, 16u);

  EXPECT_EQ(chunks[2].text, u"Third sentence?");
  EXPECT_EQ(chunks[2].start_code_unit_offset, 33u);
}

TEST_F(TextChunkerTest, SentencesWithWhitespaceSpeedMode) {
  std::u16string text = u"   Hello   .   World   ";
  std::vector<TextChunk> chunks = ChunkText(text, ChunkingMode::kSpeed);

  ASSERT_EQ(chunks.size(), 2u);
  EXPECT_EQ(chunks[0].text, u"Hello   .");
  EXPECT_EQ(chunks[0].start_code_unit_offset, 3u);

  EXPECT_EQ(chunks[1].text, u"World");
  EXPECT_EQ(chunks[1].start_code_unit_offset, 15u);
}

TEST_F(TextChunkerTest, MultiByteCharactersSpeedMode) {
  std::u16string text = u"こんにちは。元気ですか？ はい。";
  std::vector<TextChunk> chunks = ChunkText(
      text, ChunkingMode::kSpeed, GetKnownLanguageTag("ja-JP"));

  ASSERT_EQ(chunks.size(), 3u);
  EXPECT_EQ(chunks[0].text, u"こんにちは。");
  EXPECT_EQ(chunks[0].start_code_unit_offset, 0u);

  EXPECT_EQ(chunks[1].text, u"元気ですか？");
  EXPECT_EQ(chunks[1].start_code_unit_offset, 6u);

  EXPECT_EQ(chunks[2].text, u"はい。");
  EXPECT_EQ(chunks[2].start_code_unit_offset, 13u);
}

TEST_F(TextChunkerTest, AbbreviationsSpeedMode) {
  std::u16string text = u"Mr. Smith went home. He was tired.";
  std::vector<TextChunk> chunks = ChunkText(
      text, ChunkingMode::kSpeed, GetKnownLanguageTag("en-US"));

  ASSERT_EQ(chunks.size(), 3u);
  EXPECT_EQ(chunks[0].text, u"Mr.");
  EXPECT_EQ(chunks[0].start_code_unit_offset, 0u);

  EXPECT_EQ(chunks[1].text, u"Smith went home.");
  EXPECT_EQ(chunks[1].start_code_unit_offset, 4u);

  EXPECT_EQ(chunks[2].text, u"He was tired.");
  EXPECT_EQ(chunks[2].start_code_unit_offset, 21u);
}

TEST_F(TextChunkerTest, WhitespaceOnlyReturnsEmpty) {
  EXPECT_TRUE(ChunkText(u"   ", ChunkingMode::kSpeed).empty());
}

TEST_F(TextChunkerTest, DefaultLocaleNulloptSpeedMode) {
  std::u16string text = u"First sentence. Second sentence.";
  std::vector<TextChunk> chunks = ChunkText(
      text, ChunkingMode::kSpeed, /*locale_tag=*/std::nullopt);
  ASSERT_EQ(chunks.size(), 2u);
  EXPECT_EQ(chunks[0].text, u"First sentence.");
  EXPECT_EQ(chunks[1].text, u"Second sentence.");
}

TEST_F(TextChunkerTest, CustomLocaleExplicitTagSpeedMode) {
  std::u16string text = u"こんにちは。元気ですか？";
  std::vector<TextChunk> chunks = ChunkText(
      text, ChunkingMode::kSpeed, GetKnownLanguageTag("ja-JP"));
  ASSERT_EQ(chunks.size(), 2u);
  EXPECT_EQ(chunks[0].text, u"こんにちは。");
  EXPECT_EQ(chunks[1].text, u"元気ですか？");
}

TEST_F(TextChunkerTest, QualityEmptyStringReturnsEmpty) {
  std::vector<TextChunk> chunks = ChunkText(
      u"", ChunkingMode::kQuality, base::i18n::GetKnownLanguageTag("en-US"));
  EXPECT_TRUE(chunks.empty());
}

TEST_F(TextChunkerTest, QualityWhitespaceOnlyReturnsEmpty) {
  std::vector<TextChunk> chunks =
      ChunkText(u"   \n\t  ", ChunkingMode::kQuality,
                base::i18n::GetKnownLanguageTag("en-US"));
  EXPECT_TRUE(chunks.empty());
}

TEST_F(TextChunkerTest, QualitySmallSentencesGrouped) {
  std::u16string input = u"First sentence. Second sentence! Third sentence?";
  std::vector<TextChunk> chunks = ChunkText(
      input, ChunkingMode::kQuality, base::i18n::GetKnownLanguageTag("en-US"));

  ASSERT_EQ(chunks.size(), 1u);
  EXPECT_EQ(chunks[0].text,
            u"First sentence. Second sentence! Third sentence?");
  EXPECT_EQ(chunks[0].start_code_unit_offset, 0u);
}

TEST_F(TextChunkerTest, QualityParagraphBoundaryRespect) {
  std::u16string input = u"First paragraph sentence.\nSecond paragraph sentence.";
  std::vector<TextChunk> chunks = ChunkText(
      input, ChunkingMode::kQuality, base::i18n::GetKnownLanguageTag("en-US"));

  ASSERT_EQ(chunks.size(), 2u);
  EXPECT_EQ(chunks[0].text, u"First paragraph sentence.");
  EXPECT_EQ(chunks[0].start_code_unit_offset, 0u);

  EXPECT_EQ(chunks[1].text, u"Second paragraph sentence.");
  EXPECT_EQ(chunks[1].start_code_unit_offset, 26u);
}

TEST_F(TextChunkerTest, QualitySingleSentenceExceedsThreshold) {
  std::u16string long_sentence(500, u'A');
  long_sentence += u".";
  std::u16string input = long_sentence + u" Short sentence.";

  std::vector<TextChunk> chunks = ChunkText(
      input, ChunkingMode::kQuality, base::i18n::GetKnownLanguageTag("en-US"));

  ASSERT_EQ(chunks.size(), 2u);
  EXPECT_EQ(chunks[0].text, long_sentence);
  EXPECT_EQ(chunks[0].start_code_unit_offset, 0u);

  EXPECT_EQ(chunks[1].text, u"Short sentence.");
  EXPECT_EQ(chunks[1].start_code_unit_offset, 502u);
}

TEST_F(TextChunkerTest, QualityLeadingAndTrailingNewlines) {
  std::u16string input = u"\n\n  First paragraph.  \n\n  Second paragraph.  \n\n";
  std::vector<TextChunk> chunks = ChunkText(
      input, ChunkingMode::kQuality, base::i18n::GetKnownLanguageTag("en-US"));

  ASSERT_EQ(chunks.size(), 2u);
  EXPECT_EQ(chunks[0].text, u"First paragraph.");
  EXPECT_EQ(chunks[0].start_code_unit_offset, 4u);

  EXPECT_EQ(chunks[1].text, u"Second paragraph.");
  EXPECT_EQ(chunks[1].start_code_unit_offset, 26u);
}

TEST_F(TextChunkerTest, QualityUnicodeAndSurrogatePairs) {
  std::u16string input = u"Hello 𐐏 world. Next 𐐏 sentence.\nNew 𐐏 paragraph.";
  std::vector<TextChunk> chunks = ChunkText(
      input, ChunkingMode::kQuality, base::i18n::GetKnownLanguageTag("en-US"));

  ASSERT_EQ(chunks.size(), 2u);
  EXPECT_EQ(chunks[0].text, u"Hello 𐐏 world. Next 𐐏 sentence.");
  EXPECT_EQ(chunks[0].start_code_unit_offset, 0u);

  EXPECT_EQ(chunks[1].text, u"New 𐐏 paragraph.");
  EXPECT_EQ(chunks[1].start_code_unit_offset, 34u);
}

TEST_F(TextChunkerTest, QualityWindowsCRLFParagraphBoundary) {
  std::u16string input = u"First paragraph sentence.\r\nSecond paragraph sentence.";
  std::vector<TextChunk> chunks = ChunkText(
      input, ChunkingMode::kQuality, base::i18n::GetKnownLanguageTag("en-US"));

  ASSERT_EQ(chunks.size(), 2u);
  EXPECT_EQ(chunks[0].text, u"First paragraph sentence.");
  EXPECT_EQ(chunks[0].start_code_unit_offset, 0u);

  EXPECT_EQ(chunks[1].text, u"Second paragraph sentence.");
  EXPECT_EQ(chunks[1].start_code_unit_offset, 27u);
}

TEST_F(TextChunkerTest, QualityMultipleSentencesAccumulateExceedingThreshold) {
  std::u16string s1(249, u'A');
  s1 += u".";
  std::u16string s2(249, u'B');
  s2 += u".";
  std::u16string input = s1 + u" " + s2;

  std::vector<TextChunk> chunks = ChunkText(
      input, ChunkingMode::kQuality, base::i18n::GetKnownLanguageTag("en-US"));

  ASSERT_EQ(chunks.size(), 2u);
  EXPECT_EQ(chunks[0].text, s1);
  EXPECT_EQ(chunks[0].start_code_unit_offset, 0u);

  EXPECT_EQ(chunks[1].text, s2);
  EXPECT_EQ(chunks[1].start_code_unit_offset, 251u);
}

}  // namespace readaloud
