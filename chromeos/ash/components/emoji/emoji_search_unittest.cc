// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/emoji/emoji_search.h"

#include "base/containers/span.h"
#include "build/branding_buildflags.h"
#include "chromeos/ash/components/emoji/grit/emoji.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"

namespace emoji {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::FloatNear;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

struct FakeResource {
  int resource;
  std::string data;
};

class ScopedFakeResourceBundleDelegate {
 public:
  explicit ScopedFakeResourceBundleDelegate(
      base::span<const FakeResource> resources) {
    original_resource_bundle_ =
        ui::ResourceBundle::SwapSharedInstanceForTesting(nullptr);
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        "en-US", &delegate_, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);

    for (const auto& [resource, data] : resources) {
      ON_CALL(delegate_, LoadDataResourceString(resource))
          .WillByDefault(Return(data));
    }
  }

  ~ScopedFakeResourceBundleDelegate() {
    ui::ResourceBundle::CleanupSharedInstance();
    ui::ResourceBundle::SwapSharedInstanceForTesting(original_resource_bundle_);
  }

 private:
  testing::NiceMock<ui::MockResourceBundleDelegate> delegate_;
  raw_ptr<ui::ResourceBundle> original_resource_bundle_;
};

using EmojiSearchTest = testing::Test;

TEST_F(EmojiSearchTest, FindsSmilingEmojiInJapaneseLocale) {
  // Requires English strings since they are loaded first on startup.
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
        FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_JA,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow","keywords":["矢印"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[
              {"base":{"string":":-)","name":"smiley face "}}]}])-"},
        FakeResource{
            IDR_EMOJI_PICKER_JA_START,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["笑顔",":smile:"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_JA_REMAINING,
                     R"([{"emoji":[{"base":{"string":"😺","name":"grinning cat",
            "keywords":["笑顔",":smile:"]}}]}])"}}});

  EmojiSearch search;

  search.LoadEmojiLanguages({{"ja"}});
  EmojiSearchResult result = search.SearchEmoji(u"笑顔", {{"ja"}});
  EXPECT_THAT(result.emojis, UnorderedElementsAre(FieldsAre(Gt(0), "😀"),
                                                  FieldsAre(Gt(0), "😺")));
  EXPECT_THAT(result.symbols, IsEmpty());
  EXPECT_THAT(result.emoticons, IsEmpty());
}

TEST_F(EmojiSearchTest, FindsSymbolInJapaneseLocale) {
  // Requires English strings since they are loaded first on startup.
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
        FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[
              {"base":{"string":":-)","name":"smiley face "}}]}])-"},
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_JA,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow","keywords":["矢印"]}}]}])"},
        FakeResource{
            IDR_EMOJI_PICKER_JA_START,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["笑顔",":smile:"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_JA_REMAINING,
                     R"([{"emoji":[{"base":{"string":"😺","name":"grinning cat",
            "keywords":["笑顔",":smile:"]}}]}])"}}});

  EmojiSearch search;

  search.LoadEmojiLanguages({{"ja"}});
  EmojiSearchResult result = search.SearchEmoji(u"矢印", {{"ja"}});
  EXPECT_THAT(result.symbols, UnorderedElementsAre(FieldsAre(Gt(0), "←")));
  EXPECT_THAT(result.emojis, IsEmpty());
  EXPECT_THAT(result.emoticons, IsEmpty());
}

TEST_F(EmojiSearchTest, CanSearchMultipleLocales) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
                     R"([{"emoji":[{"base":{"string":"🎸","name":"guitar",
            "keywords":["music"]}}]}])"},
        FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Miscellaneous","emoji":[{"base":
            {"string":"♭","name":"music flat sign","keywords":["music"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[
              {"base":{"string":":-)","name":"smiley face "}}]}])-"},
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_FR,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"♯","name":"dièse","keywords":["dièse","musique"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_FR_START,
                     R"([{"emoji":[{"base":{"string":"🎹","name":"piano",
            "keywords":["musique"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_FR_REMAINING,
                     R"([{"emoji":[{"base":{"string":"😺","name":"chat",
            "keywords":["chat"]}}]}])"}}});

  EmojiSearch search;

  search.LoadEmojiLanguages({{"en", "fr"}});

  // Note that the results are to be presented in order of languages.
  EmojiSearchResult result = search.SearchEmoji(u"musi", {{"fr", "en"}});
  EXPECT_THAT(result.emojis, UnorderedElementsAre(FieldsAre(Gt(0), "🎹"),
                                                  FieldsAre(Gt(0), "🎸")));
  EXPECT_THAT(result.symbols, UnorderedElementsAre(FieldsAre(Gt(0), "♯"),
                                                   FieldsAre(Gt(0), "♭")));
  EXPECT_THAT(result.emoticons, IsEmpty());
}

TEST_F(EmojiSearchTest, FindsSmilingEmoji) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
        FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[
              {"base":{"string":":-)","name":"smiley face "}}]}])-"}}});

  EmojiSearch search;

  EmojiSearchResult result = search.SearchEmoji(u"face", {{"en"}});

  EXPECT_THAT(result.emojis, ElementsAre(FieldsAre(Gt(0), "😀")));
  EXPECT_THAT(result.emoticons, ElementsAre(FieldsAre(Gt(0), ":-)")));
  EXPECT_THAT(result.symbols, IsEmpty());
}

TEST_F(EmojiSearchTest, FindsSingleSmilingEmojiInMultipleLanguages) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
        FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":{"string":"←",
                     "name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_JA,
                     R"([{"group":"Arrows","emoji":[{"base":{"string":"←",
                     "name":"leftwards arrow","keywords":["矢印"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[
                     {"base":{"string":":-)","name":"smiley face "}}]}])-"},
        FakeResource{
            IDR_EMOJI_PICKER_JA_START,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["笑顔",":smile:"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_JA_REMAINING,
                     R"([{"emoji":[{"base":{"string":"😺","name":"grinning cat",
                     "keywords":["笑顔",":smile:"]}}]}])"}}});

  EmojiSearch search;

  search.LoadEmojiLanguages({{"en", "ja"}});
  EmojiSearchResult result =
      search.SearchEmoji(u"grinning face", {{"en", "ja"}});
  EXPECT_THAT(result.emojis, UnorderedElementsAre(FieldsAre(Gt(0), "😀")));
  EXPECT_THAT(result.symbols, IsEmpty());
  EXPECT_THAT(result.emoticons, IsEmpty());
}

TEST_F(EmojiSearchTest, FindsDifferentSmilingEmojiOrderMatchesLanguages) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
        FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":{"string":"←",
                     "name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_JA,
                     R"([{"group":"Arrows","emoji":[{"base":{"string":"←",
                     "name":"leftwards arrow","keywords":["矢印"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[
                     {"base":{"string":":-)","name":"smiley face "}}]}])-"},
        FakeResource{IDR_EMOJI_PICKER_JA_START,
                     R"([{"emoji":[{"base":{"string":"😺","name":"grinning cat",
                     "keywords":["笑顔",":smile:"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_JA_REMAINING,
                     R"([{"emoji":[{"base":{"string":"😺","name":"grinning cat",
                     "keywords":["笑顔",":smile:"]}}]}])"}}});

  EmojiSearch search;

  search.LoadEmojiLanguages({{"en", "ja"}});
  EmojiSearchResult en_ja_result =
      search.SearchEmoji(u"grinning", {{"en", "ja"}});
  EXPECT_THAT(en_ja_result.emojis,
              ElementsAre(FieldsAre(Gt(0), "😀"), FieldsAre(Gt(0), "😺")));
  EXPECT_THAT(en_ja_result.symbols, IsEmpty());
  EXPECT_THAT(en_ja_result.emoticons, IsEmpty());

  EmojiSearchResult ja_en_result =
      search.SearchEmoji(u"grinning", {{"ja", "en"}});
  EXPECT_THAT(ja_en_result.emojis,
              ElementsAre(FieldsAre(Gt(0), "😺"), FieldsAre(Gt(0), "😀")));
  EXPECT_THAT(ja_en_result.symbols, IsEmpty());
  EXPECT_THAT(ja_en_result.emoticons, IsEmpty());
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(EmojiSearchTest, FindsSmilingEmojiViaInternalEnString) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
        FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL,
                     R"([{"emoji":[{"base":{"string":"😀",
            "keywords":["lulz"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[
              {"base":{"string":":-)","name":"smiley face "}}]}])-"}}});

  EmojiSearch search;

  EmojiSearchResult result = search.SearchEmoji(u"lulz", {{"en"}});

  EXPECT_THAT(result.emojis, ElementsAre(FieldsAre(Gt(0), "😀")));
  EXPECT_THAT(result.emoticons, IsEmpty());
  EXPECT_THAT(result.symbols, IsEmpty());
}

TEST_F(EmojiSearchTest, FindsSmilingEmojiViaInternalJaString) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate({{
      FakeResource{IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
                   R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
      FakeResource{IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
                   R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
      FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL,
                   R"([{"emoji":[{"base":{"string":"😀",
            "keywords":["lulz"]}}]}])"},
      FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                   R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
      FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                   R"-([{"group":"Classic","emoji":[
              {"base":{"string":":-)","name":"smiley face "}}]}])-"},
      FakeResource{IDR_EMOJI_PICKER_SYMBOL_JA, R"([])"},
      FakeResource{IDR_EMOJI_PICKER_JA_START, R"([])"},
      FakeResource{IDR_EMOJI_PICKER_JA_REMAINING, R"([])"},
      FakeResource{IDR_EMOJI_PICKER_JA_INTERNAL,
                   R"([{"emoji":[{"base":{"string":"😺","name":"grinning cat",
                     "keywords":["笑顔",":smile:"]}}]}])"},
  }});

  EmojiSearch search;
  search.LoadEmojiLanguages({{"en", "ja"}});

  EmojiSearchResult result = search.SearchEmoji(u"笑顔", {{"ja"}});

  EXPECT_THAT(result.emojis, ElementsAre(FieldsAre(Gt(0), "😺")));
  EXPECT_THAT(result.emoticons, IsEmpty());
  EXPECT_THAT(result.symbols, IsEmpty());
}

#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_F(EmojiSearchTest, MultiKeywordPartialMatch) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
        FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[{"base":
            {"string":":-)","name":"smiley face "}}]}])-"}}});

  EmojiSearch search;

  EmojiSearchResult result = search.SearchEmoji(u"gr fa", {{"en"}});

  EXPECT_THAT(result.emojis, ElementsAre(FieldsAre(Gt(0), "😀")));
  EXPECT_THAT(result.symbols, IsEmpty());
  EXPECT_THAT(result.emoticons, IsEmpty());
}

TEST_F(EmojiSearchTest, FindsSmilingEmoticon) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
        FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[{"base":
            {"string":":-)","name":"smiley face "}}]}])-"}}});

  EmojiSearch search;

  EmojiSearchResult result = search.SearchEmoji(u"smiley", {{"en"}});

  EXPECT_THAT(result.emoticons, ElementsAre(FieldsAre(Gt(0), ":-)")));
  EXPECT_THAT(result.emojis, IsEmpty());
  EXPECT_THAT(result.symbols, IsEmpty());
}

TEST_F(EmojiSearchTest, FindsSymbol) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
        FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[{"base":
            {"string":":-)","name":"smiley face "}}]}])-"}}});
  EmojiSearch search;

  EmojiSearchResult result = search.SearchEmoji(u"left", {{"en"}});

  EXPECT_THAT(result.symbols, ElementsAre(FieldsAre(Gt(0), "←")));
  EXPECT_THAT(result.emojis, IsEmpty());
  EXPECT_THAT(result.emoticons, IsEmpty());
}

TEST_F(EmojiSearchTest, IgnoresCase) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
        FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[{"base":
            {"string":":-)","name":"smiley face "}}]}])-"}}});

  EmojiSearch search;

  EmojiSearchResult result = search.SearchEmoji(u"LEFT", {{"en"}});

  EXPECT_THAT(result.symbols, ElementsAre(FieldsAre(Gt(0), "←")));
  EXPECT_THAT(result.emojis, IsEmpty());
  EXPECT_THAT(result.emoticons, IsEmpty());
}

TEST_F(EmojiSearchTest, WholeNameScoresHigherThanPartialMatch) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
            R"([{"emoji":[{"base":{"string":"😀","name":"grinning faceandmore",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
        FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
            R"([{"emoji":[{"base":{"string":"😀a","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[{"base":
            {"string":":-)","name":"smiley face "}}]}])-"}}});

  EmojiSearch search;

  EmojiSearchResult result = search.SearchEmoji(u"grinning face", {{"en"}});

  EXPECT_THAT(result.emojis,
              ElementsAre(FieldsAre(Gt(0), "😀a"), FieldsAre(Gt(0), "😀")));
  EXPECT_THAT(result.symbols, IsEmpty());
  EXPECT_THAT(result.emoticons, IsEmpty());
}

TEST_F(EmojiSearchTest, NameMatchScoresHigherThanKeyword) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
            R"([{"emoji":[{"base":{"string":"😀","name":"something else",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
        FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
            R"([{"emoji":[{"base":{"string":"😀a","name":"grinning face",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[{"base":
            {"string":":-)","name":"smiley face "}}]}])-"}}});

  EmojiSearch search;

  EmojiSearchResult result = search.SearchEmoji(u"grinning face", {{"en"}});

  EXPECT_THAT(result.emojis,
              ElementsAre(FieldsAre(Gt(0), "😀a"), FieldsAre(Gt(0), "😀")));
  EXPECT_THAT(result.symbols, IsEmpty());
  EXPECT_THAT(result.emoticons, IsEmpty());
}

TEST_F(EmojiSearchTest, KeywordPartialScoresHigherThanFullKeywordMatch) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
            R"([{"emoji":[{"base":{"string":"😀","name":"something else",
            "keywords":["face","grin","grinning face",":D",":smile:"]}}]}])"},
        FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
            R"([{"emoji":[{"base":{"string":"😀a","name":"something else",
            "keywords":["face","grin","grinning facewithsomethingelse",":D",
            ":smile:"]}}]}])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[{"base":
            {"string":":-)","name":"smiley face "}}]}])-"}}});

  EmojiSearch search;

  EmojiSearchResult result = search.SearchEmoji(u"grinning face", {{"en"}});

  EXPECT_THAT(result.emojis,
              ElementsAre(FieldsAre(FloatNear(0.0029f, 0.00005f), "😀"),
                          FieldsAre(FloatNear(0.0021f, 0.00005f), "😀a")));
  EXPECT_THAT(result.symbols, IsEmpty());
  EXPECT_THAT(result.emoticons, IsEmpty());
}

TEST_F(EmojiSearchTest, BreakTiesByNameLength) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START, R"([])"},
        FakeResource{IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
                     R"([])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"emoji":[
                     {"base": {"string":"ab","name":"a b"}},
                     {"base": {"string":"abc","name":"a b c"}},
                     {"base": {"string":"abcd","name":"a b c d"}}
                     ]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON, R"([])"}}});

  EmojiSearch search;

  EmojiSearchResult result = search.SearchEmoji(u"a b", {{"en"}});

  EXPECT_THAT(result.symbols,
              ElementsAre(FieldsAre(_, "ab"), FieldsAre(_, "abc"),
                          FieldsAre(_, "abcd")));
}

}  // namespace
}  // namespace emoji
