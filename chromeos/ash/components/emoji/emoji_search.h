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
  EmojiSearchResult(std::vector<std::string> emojis,
                    std::vector<std::string> symbols,
                    std::vector<std::string> emoticons);
  ~EmojiSearchResult();

  std::vector<std::string> emojis;
  std::vector<std::string> symbols;
  std::vector<std::string> emoticons;
};

class EmojiSearch {
 public:
  EmojiSearch();
  ~EmojiSearch();
  EmojiSearch(const EmojiSearch&) = delete;
  EmojiSearch& operator=(const EmojiSearch&) = delete;

  [[nodiscard]] EmojiSearchResult SearchEmoji(std::string_view query);

  std::vector<std::string> AllResultsForTesting(const std::string& query);

 private:
  std::map<std::string, std::vector<EmojiSearchEntry>, std::less<>> emojis_;
  std::map<std::string, std::vector<EmojiSearchEntry>, std::less<>> emoticons_;
  std::map<std::string, std::vector<EmojiSearchEntry>, std::less<>> symbols_;
};
}  // namespace emoji

#endif  // CHROMEOS_ASH_COMPONENTS_EMOJI_EMOJI_SEARCH_H_
