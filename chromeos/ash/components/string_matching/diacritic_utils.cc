// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/string_matching/diacritic_utils.h"

#include <string>
#include <vector>

#include "base/containers/fixed_flat_map.h"

namespace ash::string_matching {

const std::u16string RemoveDiacritics(const std::u16string& str) {
  // For the initial implementation of diacritic-insensitive search:
  // 1) Intentionally only covering Latin-script accented characters.
  // 2) Only deal with 1-to-1 char mapping i.e., "æ > ae; œ > oe; Æ > AE; Œ >
  // OE" are ignored in this implementation. The implemented ones are listed
  // as below:

  // "[ á à â ä ā å ] > a; "
  // "[ Á À Â Ä Ā Å ] > A; "
  // "[ é è ê ë ē   ] > e; "
  // "[ É È Ê Ë Ē   ] > E; "
  // "[ í ì î ï ī   ] > i; "
  // "[ Í Ì Î Ï Ī   ] > I; "
  // "[ ó ò ô ö ō ø ] > o; "
  // "[ Ó Ò Ô Ö Ō Ø ] > O; "
  // "[ ú ù û ü ū   ] > u; "
  // "[ Ú Ù Û Ü Ū   ] > U; "
  // "[ ý ỳ ŷ ÿ ȳ   ] > y; "
  // "[ Ý Ỳ Ŷ Ÿ Ȳ   ] > Y; "
  // "ç > c; ñ > n; "
  // "Ç > C; Ñ > N;"

  // clang-format off
  static constexpr auto kConversionMap =
    base::MakeFixedFlatMap<char16_t, char16_t>({
      {u'á', u'a'}, {u'à', u'a'}, {u'â', u'a'}, {u'ä', u'a'}, {u'ā', u'a'}, {u'å', u'a'},
      {u'Á', u'A'}, {u'À', u'A'}, {u'Â', u'A'}, {u'Ä', u'A'}, {u'Ā', u'A'}, {u'Å', u'A'},
      {u'é', u'e'}, {u'è', u'e'}, {u'ê', u'e'}, {u'ë', u'e'}, {u'ē', u'e'},
      {u'É', u'E'}, {u'È', u'E'}, {u'Ê', u'E'}, {u'Ë', u'E'}, {u'Ē', u'E'},
      {u'í', u'i'}, {u'ì', u'i'}, {u'î', u'i'}, {u'ï', u'i'}, {u'ī', u'i'},
      {u'Í', u'I'}, {u'Ì', u'I'}, {u'Î', u'I'}, {u'Ï', u'I'}, {u'Ī', u'I'},
      {u'ó', u'o'}, {u'ò', u'o'}, {u'ô', u'o'}, {u'ö', u'o'}, {u'ō', u'o'}, {u'ø', u'o'},
      {u'Ó', u'O'}, {u'Ò', u'O'}, {u'Ô', u'O'}, {u'Ö', u'O'}, {u'Ō', u'O'}, {u'Ø', u'O'},
      {u'ú', u'u'}, {u'ù', u'u'}, {u'û', u'u'}, {u'ü', u'u'}, {u'ū', u'u'},
      {u'Ú', u'U'}, {u'Ù', u'U'}, {u'Û', u'U'}, {u'Ü', u'U'}, {u'Ū', u'U'},
      {u'ý', u'y'}, {u'ỳ', u'y'}, {u'ŷ', u'y'}, {u'ÿ', u'y'}, {u'ȳ', u'y'},
      {u'Ý', u'Y'}, {u'Ỳ', u'Y'}, {u'Ŷ', u'Y'}, {u'Ÿ', u'Y'}, {u'Ȳ', u'Y'},
      {u'ç', u'c'}, {u'Ç', u'C'}, {u'ñ', u'n'}, {u'Ñ', u'N'},
      });
  // clang-format on

  std::u16string result;
  for (auto letter : str) {
    auto* it = kConversionMap.find(letter);
    result.push_back(it == kConversionMap.end() ? letter : it->second);
  }
  return result;
}

}  // namespace ash::string_matching
