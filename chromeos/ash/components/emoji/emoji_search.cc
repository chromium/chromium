// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/emoji/emoji_search.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/containers/span.h"
#include "base/i18n/case_conversion.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/ranges/functional.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/emoji/grit/emoji.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace emoji {

namespace {
// Map from keyword -> sum of position weightings
std::map<std::string, double, std::less<>> CombineSearchTerms(
    base::span<const std::string> long_search_terms) {
  std::map<std::string, std::vector<int>, std::less<>> position_map;
  for (const std::string& long_string : long_search_terms) {
    std::vector<std::string_view> words = base::SplitStringPieceUsingSubstr(
        long_string, " ", base::WhitespaceHandling::TRIM_WHITESPACE,
        base::SplitResult::SPLIT_WANT_NONEMPTY);
    for (size_t i = 0; i < words.size(); ++i) {
      position_map[base::UTF16ToUTF8(
                       base::i18n::ToLower(base::UTF8ToUTF16(words[i])))]
          .push_back(i);
    }
  }

  std::map<std::string, double, std::less<>> ret;
  for (auto& map_entry : position_map) {
    double weight = 0;
    for (int p : map_entry.second) {
      weight += 1.0 / (1.0 + p);
    }
    ret[map_entry.first] = weight;
  }
  return ret;
}

// Convert a JSON file to a map from search term to emoji weighted by position
// in keyword / name.
void AddDataFromFileToMap(
    const int file_id_in_resources,
    std::map<std::string, std::vector<EmojiSearchEntry>, std::less<>>& map) {
  std::string json_string =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          file_id_in_resources);
  // Can be empty in certain test environments.
  if (json_string.empty()) {
    CHECK_IS_TEST();
    return;
  }

  // TODO(b/309343774): switch to JSON reading service
  std::optional<base::Value> json = base::JSONReader::Read(json_string);
  CHECK(json) << "parse failed for " << file_id_in_resources << ":"
              << json_string << "EOF";
  base::Value::List groups = std::move(*json).TakeList();
  // TODO(b/309343774): Consider using json_value_converter
  for (auto& group : groups) {
    for (const auto& emoji : *group.GetDict().FindList("emoji")) {
      const base::Value::Dict* base = emoji.GetDict().FindDict("base");
      const std::string* emoji_string = base->FindString("string");
      CHECK(emoji_string) << "All emoji should have names";
      // Gather possible search terms for the emoji
      std::vector<std::string> search_terms;
      const base::Value::List* keywords = base->FindList("keywords");
      if (keywords) {
        search_terms.reserve(keywords->size());
        for (const auto& keyword : *keywords) {
          search_terms.push_back(keyword.GetString());
        }
      }
      for (const auto& search_term : CombineSearchTerms(search_terms)) {
        // Keywords have less weighting (0.25)
        map[search_term.first].push_back(
            EmojiSearchEntry{.weighting = 0.25 * search_term.second,
                             .emoji_string = *emoji_string});
      }
      const std::string* name = base->FindString("name");
      if (name) {
        for (const auto& search_term : CombineSearchTerms({{*name}})) {
          map[search_term.first].push_back(
              // Name has full weighting (1.0)
              EmojiSearchEntry{.weighting = 1 * search_term.second,
                               .emoji_string = *emoji_string});
        }
      }
    }
  }
}

std::unordered_map<std::string, double> GetResultsFromASingleWordQuery(
    const std::map<std::string, std::vector<EmojiSearchEntry>, std::less<>>&
        map,
    const std::string_view query) {
  if (query.empty()) {
    return {};
  }
  std::unordered_map<std::string, double> scored_emoji;
  // Make search case insensitive.
  std::string lower_bound =
      base::UTF16ToUTF8(base::i18n::ToLower(base::UTF8ToUTF16(query)));
  std::string upper_bound = lower_bound;
  // will break if someone searches for some very specific char, but should be
  // fine.
  upper_bound.back() = upper_bound.back() + 1;
  // This should ensure we get everything that is a substring match
  auto upper_bound_iterator = map.upper_bound(upper_bound);
  for (auto matches = map.lower_bound(lower_bound);
       matches != upper_bound_iterator; ++matches) {
    for (const auto& match : matches->second) {
      // Will zero initialize if entry missing
      scored_emoji[match.emoji_string] +=
          match.weighting / matches->first.size();
    }
  }
  return scored_emoji;
}

std::vector<EmojiSearchEntry> GetResultsFromMap(
    const std::map<std::string, std::vector<EmojiSearchEntry>, std::less<>>&
        map,
    const std::string_view query) {
  std::vector<std::string> words = base::SplitStringUsingSubstr(
      query, " ", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  if (words.empty()) {
    return {};
  }
  std::unordered_map<std::string, double> scored_emoji =
      GetResultsFromASingleWordQuery(map, words.back());
  words.pop_back();
  for (const std::string& word : words) {
    std::unordered_map<std::string, double> newly_scored_emoji =
        GetResultsFromASingleWordQuery(map, word);
    for (const auto& already_scored_emoji : scored_emoji) {
      auto it = newly_scored_emoji.find(already_scored_emoji.first);
      if (it != newly_scored_emoji.end()) {
        scored_emoji[already_scored_emoji.first] *= it->second;
      } else {
        scored_emoji[already_scored_emoji.first] = 0;
      }
    }
  }
  std::erase_if(scored_emoji, [](auto elem) { return elem.second == 0.0; });
  std::vector<EmojiSearchEntry> ret;
  for (const auto& [emoji, weighting] : scored_emoji) {
    ret.push_back({weighting, emoji});
  }
  base::ranges::sort(
      ret, base::ranges::greater(),
      [](const EmojiSearchEntry& entry) { return entry.weighting; });
  return ret;
}

}  // namespace

EmojiSearchResult::EmojiSearchResult(std::vector<EmojiSearchEntry> emojis,
                                     std::vector<EmojiSearchEntry> symbols,
                                     std::vector<EmojiSearchEntry> emoticons)
    : emojis(std::move(emojis)),
      symbols(std::move(symbols)),
      emoticons(std::move(emoticons)) {}

EmojiSearchResult::~EmojiSearchResult() = default;

EmojiSearch::EmojiSearch() {
  AddDataFromFileToMap(IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
                       emojis_);
  AddDataFromFileToMap(IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
                       emojis_);
  AddDataFromFileToMap(IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON, symbols_);
  AddDataFromFileToMap(IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON, emoticons_);
}

EmojiSearch::~EmojiSearch() = default;

EmojiSearchResult EmojiSearch::SearchEmoji(const std::string_view query) {
  return EmojiSearchResult(GetResultsFromMap(emojis_, query),
                           GetResultsFromMap(symbols_, query),
                           GetResultsFromMap(emoticons_, query));
}

std::vector<std::string> EmojiSearch::AllResultsForTesting(
    const std::string& query) {
  std::vector<std::string> ret;
  for (EmojiSearchEntry& r : GetResultsFromMap(emojis_, query)) {
    ret.push_back(std::move(r).emoji_string);
  }
  for (EmojiSearchEntry& r : GetResultsFromMap(emoticons_, query)) {
    ret.push_back(std::move(r).emoji_string);
  }
  for (EmojiSearchEntry& r : GetResultsFromMap(symbols_, query)) {
    ret.push_back(std::move(r).emoji_string);
  }
  return ret;
}

}  // namespace emoji
