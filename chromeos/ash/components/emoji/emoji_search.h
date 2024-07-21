// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_EMOJI_EMOJI_SEARCH_H_
#define CHROMEOS_ASH_COMPONENTS_EMOJI_EMOJI_SEARCH_H_

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace emoji {

// Simple struct for storing a search weighting for a particular emoji.
struct EmojiSearchEntry {
  double weighting;
  std::string emoji_string;
};

struct EmojiSearchResult {
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

  [[nodiscard]] EmojiSearchResult SearchEmoji(std::string_view query);

  bool SetEmojiLanguage(std::string_view language_code);

  // Returns an empty string if the emoji has no name.
  std::string GetEmojiName(std::string_view emoji) const;

  std::vector<std::string> AllResultsForTesting(const std::string& query);

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

  EntryMap emojis_;
  EntryMap emoticons_;
  EntryMap symbols_;

  // A mapping of emojis, emoticons, and symbols to their names in English.
  std::map<std::string, std::string, std::less<>> names_;

  std::optional<LanguageResourceIds> GetLanguageResourceIds(LanguageCode code);

  std::optional<LanguageCode> GetLanguageCode(std::string_view code);
};

}  // namespace emoji

#endif  // CHROMEOS_ASH_COMPONENTS_EMOJI_EMOJI_SEARCH_H_
