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
  double weighting;
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

class EmojiSearch {
 public:
  EmojiSearch();
  ~EmojiSearch();
  EmojiSearch(const EmojiSearch&) = delete;
  EmojiSearch& operator=(const EmojiSearch&) = delete;

  [[nodiscard]] EmojiSearchResult SearchEmoji(
      std::string_view query,
      base::span<const std::string> language_codes);

  void LoadEmojiLanguages(base::span<const std::string> language_codes);

  // Returns an empty string if the emoji has no name.
  std::string GetEmojiName(std::string_view emoji,
                           std::string_view language_code) const;

 private:
  using EntryMap =
      std::map<std::string, std::vector<EmojiSearchEntry>, std::less<>>;

  enum class LanguageCode {
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

  struct LanguageResourceIds {
    int emoji_start_resource_id;
    int emoji_remaining_resource_id;
    int symbols_resource_id;
  };

  struct LanguageData {
    LanguageData();
    ~LanguageData();
    LanguageData(LanguageData& language_data);
    LanguageData(LanguageData&& language_data);

    EntryMap emojis;
    EntryMap symbols;
    EntryMap emoticons;
    // A mapping of emojis, emoticons, and symbols to their names.
    std::map<std::string, std::string, std::less<>> names;
  };

  std::optional<LanguageResourceIds> GetLanguageResourceIds(LanguageCode code);

  std::optional<LanguageCode> GetLanguageCode(std::string_view code) const;

  void LoadLanguage(std::string_view language_code);

  std::map<LanguageCode, LanguageData> language_data_;
};
}  // namespace emoji

#endif  // CHROMEOS_ASH_COMPONENTS_EMOJI_EMOJI_SEARCH_H_
