// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/emoji/emoji_search.h"

#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
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

struct PrefixMatcher {
  std::u16string_view prefix;
};

bool operator<(PrefixMatcher a, std::u16string_view b) {
  return a.prefix < b.substr(0, a.prefix.size());
}

bool operator<(std::u16string_view a, PrefixMatcher b) {
  return a.substr(0, b.prefix.size()) < b.prefix;
}

// Represents the 'cost' of an emoji match, in the sense of a cost function.
// The lower the cost, the more relevant the match.
struct EmojiMatchCost {
  // Costs are compared by the language cost first (the lower the better).
  // The relevance cost is only used when the language costs are equal.
  // The name length (the lower the better) is used when both the language and
  // relevance scores are equal.
  int language_cost;
  // This value should be negative.
  float relevance_cost;
  int name_length = 0;

  auto operator<=>(const EmojiMatchCost& other) const = default;
};

// Map from keyword -> sum of position weightings
std::map<std::u16string, float, std::less<>> CombineSearchTerms(
    base::span<const std::string_view> long_search_terms) {
  std::map<std::u16string, float, std::less<>> ret;
  for (std::string_view long_string : long_search_terms) {
    std::vector<std::string_view> words = base::SplitStringPieceUsingSubstr(
        long_string, " ", base::WhitespaceHandling::TRIM_WHITESPACE,
        base::SplitResult::SPLIT_WANT_NONEMPTY);
    for (size_t i = 0; i < words.size(); ++i) {
      ret[base::i18n::ToLower(base::UTF8ToUTF16(words[i]))] += 1.0 / (1.0 + i);
    }
  }

  return ret;
}

// Convert a JSON file to a map from search term to emoji weighted by
// position in keyword / name, as well as storing names in `names`.
void AddDataFromFileToMap(
    const int file_id_in_resources,
    EmojiEntryMap& map,
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
      // Gather possible search terms for the emoji
      std::vector<std::string_view> search_terms;
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
            EmojiSearchEntry{.weighting = 0.25f * search_term.second,
                             .emoji_string = *emoji_string});
      }
      const std::string* name = base->FindString("name");
      if (name) {
        for (const auto& search_term : CombineSearchTerms({{*name}})) {
          map[search_term.first].push_back(
              // Name has full weighting (1.0)
              EmojiSearchEntry{.weighting = 1.0f * search_term.second,
                               .emoji_string = *emoji_string});
        }
        names.emplace(*emoji_string, *name);
      }
    }
  }
}

std::map<std::string_view, EmojiMatchCost> GetResultsFromMap(
    const EmojiEntryMap& map,
    base::span<const std::u16string_view> lowercase_words,
    std::map<std::string, std::string, std::less<>> names,
    int language_cost) {
  std::map<std::string_view, EmojiMatchCost> emoji_costs;
  for (const std::u16string_view lowercase_word : lowercase_words) {
    std::map<std::string_view, EmojiMatchCost> emoji_costs_for_word;
    for (auto [matches, end] = map.equal_range(PrefixMatcher{lowercase_word});
         matches != end; ++matches) {
      for (const auto& match : matches->second) {
        float previous_cost;
        if (emoji_costs.empty()) {
          // First word.
          previous_cost = -1;
        } else if (const auto& it = emoji_costs.find(match.emoji_string);
                   it != emoji_costs.end()) {
          // Second+ word, and emoji was previously found.
          previous_cost = it->second.relevance_cost;
        } else {
          // Second+ word, and emoji was not previously found.
          continue;
        }
        // Will zero initialize if entry missing
        EmojiMatchCost& cost = emoji_costs_for_word[match.emoji_string];
        cost.language_cost = language_cost;
        cost.relevance_cost +=
            previous_cost * match.weighting / matches->first.size();
      }
    }
    if (emoji_costs_for_word.empty()) {
      // Early return if there were no matches, as we assume an empty
      // `emoji_costs` means the first word.
      break;
    }
    emoji_costs = std::move(emoji_costs_for_word);
  }
  for (auto& [emoji, cost] : emoji_costs) {
    if (auto it = names.find(emoji); it != names.end()) {
      cost.name_length = it->second.length();
    }
  }
  return emoji_costs;
}

std::vector<EmojiSearchEntry> SortEmojiResultsByCost(
    std::map<std::string_view, EmojiMatchCost> emoji_costs) {
  std::vector<std::pair<EmojiMatchCost, std::string_view>> emojis_by_cost;
  emojis_by_cost.reserve(emoji_costs.size());
  base::ranges::transform(emoji_costs, std::back_inserter(emojis_by_cost),
                          [](const auto& entry) {
                            return std::make_pair(entry.second, entry.first);
                          });
  base::ranges::sort(emojis_by_cost);
  std::vector<EmojiSearchEntry> ret;
  ret.reserve(emojis_by_cost.size());
  base::ranges::transform(emojis_by_cost, std::back_inserter(ret),
                          [](const auto& entry) {
                            return EmojiSearchEntry{-entry.first.relevance_cost,
                                                    std::string(entry.second)};
                          });
  return ret;
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
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
               .emoji_internal_resource_id = IDR_EMOJI_PICKER_DA_INTERNAL,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
           }},
          {EmojiLanguageCode::kDe,
           {
               .emoji_start_resource_id = IDR_EMOJI_PICKER_DE_START,
               .emoji_remaining_resource_id = IDR_EMOJI_PICKER_DE_REMAINING,
               .symbols_resource_id = IDR_EMOJI_PICKER_SYMBOL_DE,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
               .emoji_internal_resource_id = IDR_EMOJI_PICKER_DE_INTERNAL,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
           }},
          {EmojiLanguageCode::kEn,
           {
               .emoji_start_resource_id =
                   IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
               .emoji_remaining_resource_id =
                   IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
               .symbols_resource_id = IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
               .emoji_internal_resource_id = IDR_EMOJI_PICKER_EN_INTERNAL,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
           }},
          {EmojiLanguageCode::kEs,
           {
               .emoji_start_resource_id = IDR_EMOJI_PICKER_ES_START,
               .emoji_remaining_resource_id = IDR_EMOJI_PICKER_ES_REMAINING,
               .symbols_resource_id = IDR_EMOJI_PICKER_SYMBOL_ES,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
               .emoji_internal_resource_id = IDR_EMOJI_PICKER_ES_INTERNAL,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
           }},
          {EmojiLanguageCode::kFi,
           {
               .emoji_start_resource_id = IDR_EMOJI_PICKER_FI_START,
               .emoji_remaining_resource_id = IDR_EMOJI_PICKER_FI_REMAINING,
               .symbols_resource_id = IDR_EMOJI_PICKER_SYMBOL_FI,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
               .emoji_internal_resource_id = IDR_EMOJI_PICKER_FI_INTERNAL,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
           }},
          {EmojiLanguageCode::kFr,
           {
               .emoji_start_resource_id = IDR_EMOJI_PICKER_FR_START,
               .emoji_remaining_resource_id = IDR_EMOJI_PICKER_FR_REMAINING,
               .symbols_resource_id = IDR_EMOJI_PICKER_SYMBOL_FR,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
               .emoji_internal_resource_id = IDR_EMOJI_PICKER_FR_INTERNAL,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
           }},

          {EmojiLanguageCode::kJa,
           {
               .emoji_start_resource_id = IDR_EMOJI_PICKER_JA_START,
               .emoji_remaining_resource_id = IDR_EMOJI_PICKER_JA_REMAINING,
               .symbols_resource_id = IDR_EMOJI_PICKER_SYMBOL_JA,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
               .emoji_internal_resource_id = IDR_EMOJI_PICKER_JA_INTERNAL,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
           }},
          {EmojiLanguageCode::kNo,
           {
               .emoji_start_resource_id = IDR_EMOJI_PICKER_NO_START,
               .emoji_remaining_resource_id = IDR_EMOJI_PICKER_NO_REMAINING,
               .symbols_resource_id = IDR_EMOJI_PICKER_SYMBOL_NO,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
               .emoji_internal_resource_id = IDR_EMOJI_PICKER_NO_INTERNAL,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
           }},
          {EmojiLanguageCode::kSv,
           {
               .emoji_start_resource_id = IDR_EMOJI_PICKER_SV_START,
               .emoji_remaining_resource_id = IDR_EMOJI_PICKER_SV_REMAINING,
               .symbols_resource_id = IDR_EMOJI_PICKER_SYMBOL_SV,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
               .emoji_internal_resource_id = IDR_EMOJI_PICKER_SV_INTERNAL,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
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
    std::u16string_view query,
    base::span<const std::string> language_codes,
    std::optional<size_t> max_emojis,
    std::optional<size_t> max_symbols,
    std::optional<size_t> max_emoticons) {
  std::map<std::string_view, EmojiMatchCost> emojis;
  std::map<std::string_view, EmojiMatchCost> symbols;
  std::map<std::string_view, EmojiMatchCost> emoticons;

  // Make search case insensitive.
  std::u16string lowercase_query = base::i18n::ToLower(query);
  std::vector<std::u16string_view> lowercase_words =
      base::SplitStringPieceUsingSubstr(
          lowercase_query, u" ", base::WhitespaceHandling::TRIM_WHITESPACE,
          base::SplitResult::SPLIT_WANT_NONEMPTY);

  // `language_codes` are sorted in order of preference, so start with a low
  // language cost then go up.
  int language_cost = 0;
  for (const std::string& code_str : language_codes) {
    std::optional<EmojiLanguageCode> code = GetLanguageCode(code_str);
    if (!code.has_value()) {
      continue;
    }
    if (const auto& it = language_data_.find(*code);
        it != language_data_.end()) {
      // Languages are ordered by preference, so once there are enough results,
      // we can stop adding to the map.
      if (!max_emojis.has_value() || emojis.size() < *max_emojis) {
        emojis.merge(GetResultsFromMap(it->second.emojis, lowercase_words,
                                       it->second.names, language_cost));
      }
      if (!max_symbols.has_value() || symbols.size() < *max_symbols) {
        symbols.merge(GetResultsFromMap(it->second.symbols, lowercase_words,
                                        it->second.names, language_cost));
      }
      if (!max_emoticons.has_value() || emoticons.size() < *max_emoticons) {
        emoticons.merge(GetResultsFromMap(it->second.emoticons, lowercase_words,
                                          it->second.names, language_cost));
      }
      ++language_cost;
    }
  }
  return EmojiSearchResult(SortEmojiResultsByCost(emojis),
                           SortEmojiResultsByCost(symbols),
                           SortEmojiResultsByCost(emoticons));
}

void EmojiSearch::LoadEmojiLanguages(
    base::span<const std::string> language_codes) {
  for (const std::string& code : language_codes) {
    LoadLanguage(code);
  }
}

void EmojiSearch::LoadLanguage(std::string_view language_code) {
  std::optional<EmojiLanguageCode> lang = GetLanguageCode(language_code);
  if (!lang.has_value()) {
    return;
  }

  auto [it, inserted] = language_data_.emplace(*lang, EmojiLanguageData());
  if (!inserted) {
    return;
  }
  EmojiLanguageData& new_data = it->second;

  if (std::optional<EmojiLanguageResourceIds> resource_ids =
          GetLanguageResourceIds(*lang);
      resource_ids.has_value()) {
    AddDataFromFileToMap(resource_ids->emoji_start_resource_id, new_data.emojis,
                         new_data.names);
    AddDataFromFileToMap(resource_ids->emoji_remaining_resource_id,
                         new_data.emojis, new_data.names);
    AddDataFromFileToMap(resource_ids->symbols_resource_id, new_data.symbols,
                         new_data.names);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    AddDataFromFileToMap(resource_ids->emoji_internal_resource_id,
                         new_data.emojis, new_data.names);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

    if (*lang == EmojiLanguageCode::kEn) {
      // Only English has Emoticons.
      AddDataFromFileToMap(IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                           new_data.emoticons, new_data.names);
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
