// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/emoji/emoji_search.h"

#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/containers/fixed_flat_map.h"
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
#include "build/branding_buildflags.h"
#include "chromeos/ash/components/emoji/grit/emoji.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace emoji {

namespace {

constexpr std::string_view kDefaultLanguageCode = "en";

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

// Convert a JSON file to a map from search term to emoji weighted by
// position in keyword / name.
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

// Convert a JSON file to a map of emoji to name.
void AddNamesFromFileToMap(
    const int file_id_in_resources,
    std::map<std::string, std::string, std::less<>>& names) {
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
      const std::string* name = base->FindString("name");
      if (name) {
        names.emplace(*emoji_string, *name);
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
  // will break if someone searches for some very specific char, but
  // should be fine.
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

void MergeResults(std::vector<EmojiSearchEntry>& accumulator,
                  std::set<std::string>& seen,
                  base::span<EmojiSearchEntry> new_results) {
  for (EmojiSearchEntry& new_result : new_results) {
    auto [it, inserted] = seen.emplace(new_result.emoji_string);
    if (inserted) {
      accumulator.push_back(std::move(new_result));
    }
  }
}

std::optional<EmojiLanguageCode> GetLanguageCode(std::string_view code) {
  static constexpr auto kLangCodeStrToEnum =
      base::MakeFixedFlatMap<std::string_view, EmojiLanguageCode>({
          {"da", EmojiLanguageCode::kDa},
          {"de", EmojiLanguageCode::kDe},
          {"en", EmojiLanguageCode::kEn},
          {"es", EmojiLanguageCode::kEs},
          {"fi", EmojiLanguageCode::kFi},
          {"fr", EmojiLanguageCode::kFr},
          {"ja", EmojiLanguageCode::kJa},
          {"no", EmojiLanguageCode::kNo},
          {"sv", EmojiLanguageCode::kSv},
      });

  if (const auto& it = kLangCodeStrToEnum.find(code);
      it != kLangCodeStrToEnum.end()) {
    return std::optional<EmojiLanguageCode>(it->second);
  }

  return std::nullopt;
}

std::optional<EmojiLanguageResourceIds> GetLanguageResourceIds(
    EmojiLanguageCode code) {
  static constexpr auto kLanguageToData =
      base::MakeFixedFlatMap<EmojiLanguageCode, EmojiLanguageResourceIds>({
          {EmojiLanguageCode::kDa,
           {
               .emoji_start_resource_id = IDR_EMOJI_PICKER_DA_START,
               .emoji_remaining_resource_id = IDR_EMOJI_PICKER_DA_REMAINING,
               .symbols_resource_id = IDR_EMOJI_PICKER_SYMBOL_DA,
           }},
          {EmojiLanguageCode::kDe,
           {
               .emoji_start_resource_id = IDR_EMOJI_PICKER_DE_START,
               .emoji_remaining_resource_id = IDR_EMOJI_PICKER_DE_REMAINING,
               .symbols_resource_id = IDR_EMOJI_PICKER_SYMBOL_DE,
           }},
          {EmojiLanguageCode::kEn,
           {
               .emoji_start_resource_id =
                   IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
               .emoji_remaining_resource_id =
                   IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
               .symbols_resource_id = IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
           }},
          {EmojiLanguageCode::kEs,
           {
               .emoji_start_resource_id = IDR_EMOJI_PICKER_ES_START,
               .emoji_remaining_resource_id = IDR_EMOJI_PICKER_ES_REMAINING,
               .symbols_resource_id = IDR_EMOJI_PICKER_SYMBOL_ES,
           }},
          {EmojiLanguageCode::kFi,
           {
               .emoji_start_resource_id = IDR_EMOJI_PICKER_FI_START,
               .emoji_remaining_resource_id = IDR_EMOJI_PICKER_FI_REMAINING,
               .symbols_resource_id = IDR_EMOJI_PICKER_SYMBOL_FI,
           }},
          {EmojiLanguageCode::kFr,
           {
               .emoji_start_resource_id = IDR_EMOJI_PICKER_FR_START,
               .emoji_remaining_resource_id = IDR_EMOJI_PICKER_FR_REMAINING,
               .symbols_resource_id = IDR_EMOJI_PICKER_SYMBOL_FR,
           }},

          {EmojiLanguageCode::kJa,
           {
               .emoji_start_resource_id = IDR_EMOJI_PICKER_JA_START,
               .emoji_remaining_resource_id = IDR_EMOJI_PICKER_JA_REMAINING,
               .symbols_resource_id = IDR_EMOJI_PICKER_SYMBOL_JA,
           }},
          {EmojiLanguageCode::kNo,
           {
               .emoji_start_resource_id = IDR_EMOJI_PICKER_NO_START,
               .emoji_remaining_resource_id = IDR_EMOJI_PICKER_NO_REMAINING,
               .symbols_resource_id = IDR_EMOJI_PICKER_SYMBOL_NO,
           }},
          {EmojiLanguageCode::kSv,
           {
               .emoji_start_resource_id = IDR_EMOJI_PICKER_SV_START,
               .emoji_remaining_resource_id = IDR_EMOJI_PICKER_SV_REMAINING,
               .symbols_resource_id = IDR_EMOJI_PICKER_SYMBOL_SV,
           }},
      });

  if (const auto& it = kLanguageToData.find(code);
      it != kLanguageToData.end()) {
    return std::optional<EmojiLanguageResourceIds>(it->second);
  }

  return std::nullopt;
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
  // Adds default language emoji data on startup.
  LoadLanguage(kDefaultLanguageCode);
}

EmojiSearch::~EmojiSearch() = default;

EmojiLanguageData::EmojiLanguageData() = default;
EmojiLanguageData::~EmojiLanguageData() = default;
EmojiLanguageData::EmojiLanguageData(EmojiLanguageData& emoji_language_data) =
    default;
EmojiLanguageData::EmojiLanguageData(EmojiLanguageData&& emoji_language_data) =
    default;

EmojiSearchResult EmojiSearch::SearchEmoji(
    std::string_view query,
    base::span<const std::string> language_codes) {
  std::vector<EmojiSearchEntry> emojis;
  std::set<std::string> seen_emojis;
  std::vector<EmojiSearchEntry> symbols;
  std::set<std::string> seen_symbols;
  std::vector<EmojiSearchEntry> emoticons;
  std::set<std::string> seen_emoticons;

  for (const std::string& code_str : language_codes) {
    std::optional<EmojiLanguageCode> code = GetLanguageCode(code_str);
    if (!code.has_value()) {
      continue;
    }
    if (const auto& it = language_data_.find(*code);
        it != language_data_.end()) {
      std::vector<EmojiSearchEntry> new_emojis =
          GetResultsFromMap(it->second.emojis, query);
      MergeResults(emojis, seen_emojis, new_emojis);
      std::vector<EmojiSearchEntry> new_symbols =
          GetResultsFromMap(it->second.symbols, query);
      MergeResults(symbols, seen_symbols, new_symbols);
      std::vector<EmojiSearchEntry> new_emoticons =
          GetResultsFromMap(it->second.emoticons, query);
      MergeResults(emoticons, seen_emoticons, new_emoticons);
    }
  }
  return EmojiSearchResult(emojis, symbols, emoticons);
}

void EmojiSearch::LoadEmojiLanguages(
    base::span<const std::string> language_codes) {
  for (const std::string& code : language_codes) {
    LoadLanguage(code);
  }
}

void EmojiSearch::LoadLanguage(std::string_view language_code) {
  std::optional<EmojiLanguageCode> lang = GetLanguageCode(language_code);
  if (!lang.has_value() || language_data_.contains(*lang)) {
    return;
  }

  language_data_.emplace(*lang, EmojiLanguageData());
  EmojiLanguageData& new_data = language_data_.at(*lang);

  if (std::optional<EmojiLanguageResourceIds> resource_ids =
          GetLanguageResourceIds(*lang);
      resource_ids.has_value()) {
    AddDataFromFileToMap(resource_ids->emoji_start_resource_id,
                         new_data.emojis);
    AddDataFromFileToMap(resource_ids->emoji_remaining_resource_id,
                         new_data.emojis);
    AddDataFromFileToMap(resource_ids->symbols_resource_id, new_data.symbols);

    AddNamesFromFileToMap(resource_ids->emoji_start_resource_id,
                          new_data.names);
    AddNamesFromFileToMap(resource_ids->emoji_remaining_resource_id,
                          new_data.names);
    AddNamesFromFileToMap(resource_ids->symbols_resource_id, new_data.names);

    if (*lang == EmojiLanguageCode::kEn) {
      // Only English has Emoticons.
      AddDataFromFileToMap(IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                           new_data.emoticons);
      AddNamesFromFileToMap(IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                            new_data.names);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      // English has special internal emoji strings.
      AddDataFromFileToMap(IDR_EMOJI_PICKER_EN_INTERNAL, new_data.emojis);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    }
  }

  return;
}

std::string EmojiSearch::GetEmojiName(std::string_view emoji,
                                      std::string_view language_code) const {
  std::optional<EmojiLanguageCode> lang = GetLanguageCode(language_code);

  if (!lang.has_value()) {
    return "";
  }

  if (const auto& data_it = language_data_.find(*lang);
      data_it != language_data_.end()) {
    if (const auto it = data_it->second.names.find(emoji);
        it != data_it->second.names.end()) {
      return it->second;
    }
  }

  return "";
}

}  // namespace emoji
