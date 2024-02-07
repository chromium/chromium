// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/emoji/emoji_search.h"

#include "base/containers/span.h"
#include "chromeos/ash/components/emoji/grit/emoji.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"

namespace emoji {
namespace {

using ::testing::ElementsAre;
using ::testing::Return;

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
      EXPECT_CALL(delegate_, LoadDataResourceString(resource))
          .WillOnce(Return(data));
    }
  }

  ~ScopedFakeResourceBundleDelegate() {
    ui::ResourceBundle::CleanupSharedInstance();
    ui::ResourceBundle::SwapSharedInstanceForTesting(original_resource_bundle_);
  }

 private:
  ui::MockResourceBundleDelegate delegate_;
  raw_ptr<ui::ResourceBundle> original_resource_bundle_;
};

using EmojiSearchTest = testing::Test;

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
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[
              {"base":{"string":":-)","name":"smiley face "}}]}])-"}}});

  EmojiSearch search;

  std::vector<std::string> results = search.AllResultsForTesting("face");

  EXPECT_THAT(results, ElementsAre("😀", ":-)"));
}

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
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[{"base":
            {"string":":-)","name":"smiley face "}}]}])-"}}});

  EmojiSearch search;

  std::vector<std::string> results = search.AllResultsForTesting("gr fa");

  EXPECT_THAT(results, ElementsAre("😀"));
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
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[{"base":
            {"string":":-)","name":"smiley face "}}]}])-"}}});

  EmojiSearch search;

  std::vector<std::string> results = search.AllResultsForTesting("smiley");

  EXPECT_THAT(results, ElementsAre(":-)"));
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
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[{"base":
            {"string":":-)","name":"smiley face "}}]}])-"}}});
  EmojiSearch search;

  std::vector<std::string> results = search.AllResultsForTesting("left");

  EXPECT_THAT(results, ElementsAre("←"));
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
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[{"base":
            {"string":":-)","name":"smiley face "}}]}])-"}}});

  EmojiSearch search;

  std::vector<std::string> results = search.AllResultsForTesting("LEFT");

  EXPECT_THAT(results, ElementsAre("←"));
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
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[{"base":
            {"string":":-)","name":"smiley face "}}]}])-"}}});

  EmojiSearch search;

  std::vector<std::string> results =
      search.AllResultsForTesting("grinning face");

  EXPECT_THAT(results, ElementsAre("😀a", "😀"));
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
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[{"base":
            {"string":":-)","name":"smiley face "}}]}])-"}}});

  EmojiSearch search;

  std::vector<std::string> results =
      search.AllResultsForTesting("grinning face");

  EXPECT_THAT(results, ElementsAre("😀a", "😀"));
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
            "keywords":["face","grin","grinning face with something else",":D",":smile:"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[{"base":
            {"string":":-)","name":"smiley face "}}]}])-"}}});

  EmojiSearch search;

  std::vector<std::string> results =
      search.AllResultsForTesting("grinning face");

  EXPECT_THAT(results, ElementsAre("😀", "😀a"));
}

}  // namespace
}  // namespace emoji
