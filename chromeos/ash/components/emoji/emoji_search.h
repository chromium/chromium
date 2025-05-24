// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_EMOJI_EMOJI_SEARCH_H_
#define CHROMEOS_ASH_COMPONENTS_EMOJI_EMOJI_SEARCH_H_

#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"

namespace emoji {

// Simple struct for storing a search weighting for a particular emoji.
struct EmojiSearchEntry {
  float weighting;
  std::string emoji_string;

  friend void PrintTo(const EmojiSearchEntry& entry, std::ostream* os) {
    *os << "(" << entry.weighting << "," << entry.emoji_string << ")";
  }
};

struct EmojiSearchResult {
  EmojiSearchResult();
  EmojiSearchResult(std::vector<EmojiSearchEntry> emojis,
                    std::vector<EmojiSearchEntry> symbols,
                    std::vector<EmojiSearchEntry> emoticons);
  ~EmojiSearchResult();

  std::vector<EmojiSearchEntry> emojis;
  std::vector<EmojiSearchEntry> symbols;
  std::vector<EmojiSearchEntry> emoticons;
};

using EmojiEntryMap =
    std::map<std::u16string, std::vector<EmojiSearchEntry>, std::less<>>;

enum class EmojiLanguageCode {
  kDa,  // Danish
  kDe,  // German
  kEn,  // English
  kEs,  // Spanish
  kFi,  // Finnish
  kFr,  // French
  kJa,  // Japanese
  kNo,  // Norweigian
  kSv,  // Swedish
};

struct EmojiLanguageResourceIds {
  int emoji_start_resource_id;
  int emoji_remaining_resource_id;
  int symbols_resource_id;
  int emoji_internal_resource_id;
};

struct EmojiLanguageData {
  EmojiLanguageData();
  ~EmojiLanguageData();
  EmojiLanguageData(EmojiLanguageData& language_data);
  EmojiLanguageData(EmojiLanguageData&& language_data);

  EmojiEntryMap emojis;
  EmojiEntryMap symbols;
  EmojiEntryMap emoticons;
  // A mapping of emojis, emoticons, and symbols to their names.
  std::map<std::string, std::string, std::less<>> names;
};

class EmojiSearch {
 public:
  EmojiSearch();
  ~EmojiSearch();
  EmojiSearch(const EmojiSearch&) = delete;
  EmojiSearch& operator=(const EmojiSearch&) = delete;

  // If multiple `language_codes` are provided, this aggregates the search
  // results from multiple languages by prioritising languages earlier in the
  // `language_codes` span first, then prioritising emoji `weighting`. Because
  // of this, `weighting` is NOT guaranteed to be in non-increasing order.
  [[nodiscard]] EmojiSearchResult SearchEmoji(
      std::u16string_view query,
      base::span<const std::string> language_codes,
      std::optional<size_t> max_emojis = std::nullopt,
      std::optional<size_t> max_symbols = std::nullopt,
      std::optional<size_t> max_emoticons = std::nullopt);

  void LoadEmojiLanguages(base::span<const std::string> language_codes);

  // Returns an empty string if the emoji has no name.
  std::string GetEmojiName(std::string_view emoji,
                           std::string_view language_code) const;

 private:
  void LoadLanguage(std::string_view language_code);

  std::map<EmojiLanguageCode, EmojiLanguageData> language_data_;
};
}  // namespace emoji

#endif  // CHROMEOS_ASH_COMPONENTS_EMOJI_EMOJI_SEARCH_H_
