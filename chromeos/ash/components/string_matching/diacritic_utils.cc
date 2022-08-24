// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/diacritic_utils.h"

#include <string>
#include <vector>

#include "base/containers/flat_map.h"

namespace ash::string_matching {

// Intentionally only covering Latin-script accented characters for our initial
// implementation of diacritic-insensitive search.
DiacriticUtils::DiacriticUtils() {
  std::vector<std::pair<std::u16string, std::u16string>>
      diacritics_to_stripped = {
          {u"áàâäāå", u"a"}, {u"ÁÀÂÄĀÅ", u"A"}, {u"éèêëē", u"e"},
          {u"ÉÈÊËĒ", u"E"},  {u"íìîïī", u"i"},  {u"óòôöōø", u"o"},
          {u"úùûüū", u"u"},  {u"ÚÙÛÜŪ", u"U"},  {u"ýỳŷÿȳ", u"y"},
          {u"ÝỲŶŸȲ", u"Y"},  {u"ç", u"c"},      {u"Ç", u"C"},
          {u"ñ", u"n"},      {u"Ñ", u"N"},      {u"æ", u"ae"},
          {u"Æ", u"AE"},     {u"œ", u"oe"},     {u"Œ", u"OE"},
      };

  for (const auto& mapping_pair : diacritics_to_stripped) {
    for (const auto& diacritic : mapping_pair.first) {
      conversion_map_[diacritic] = mapping_pair.second;
    }
  }
}

DiacriticUtils::~DiacriticUtils() = default;

const std::u16string DiacriticUtils::RemoveDiacritics(
    const std::u16string& str) {
  std::u16string result;
  for (auto letter : str) {
    auto it = conversion_map_.find(letter);
    if (it == conversion_map_.end()) {
      result.push_back(letter);
    } else {
      result.append(it->second);
    }
  }
  return result;
}

}  // namespace ash::string_matching
