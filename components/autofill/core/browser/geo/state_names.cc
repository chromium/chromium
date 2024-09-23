// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/state_names.h"

#include <stddef.h>

#include <string>
#include <string_view>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace autofill::state_names {

namespace {

// TODO(jhawkins): Add more states/provinces.  See http://crbug.com/45039.
constexpr auto kStateData =
    base::MakeFixedFlatMap<std::u16string_view, std::u16string_view>(
        {{u"alabama", u"al"},
         {u"alaska", u"ak"},
         {u"arizona", u"az"},
         {u"arkansas", u"ar"},
         {u"california", u"ca"},
         {u"colorado", u"co"},
         {u"connecticut", u"ct"},
         {u"delaware", u"de"},
         {u"district of columbia", u"dc"},
         {u"florida", u"fl"},
         {u"georgia", u"ga"},
         {u"hawaii", u"hi"},
         {u"idaho", u"id"},
         {u"illinois", u"il"},
         {u"indiana", u"in"},
         {u"iowa", u"ia"},
         {u"kansas", u"ks"},
         {u"kentucky", u"ky"},
         {u"louisiana", u"la"},
         {u"maine", u"me"},
         {u"maryland", u"md"},
         {u"massachusetts", u"ma"},
         {u"michigan", u"mi"},
         {u"minnesota", u"mn"},
         {u"mississippi", u"ms"},
         {u"missouri", u"mo"},
         {u"montana", u"mt"},
         {u"nebraska", u"ne"},
         {u"nevada", u"nv"},
         {u"new hampshire", u"nh"},
         {u"new jersey", u"nj"},
         {u"new mexico", u"nm"},
         {u"new york", u"ny"},
         {u"north carolina", u"nc"},
         {u"north dakota", u"nd"},
         {u"ohio", u"oh"},
         {u"oklahoma", u"ok"},
         {u"oregon", u"or"},
         {u"pennsylvania", u"pa"},
         {u"puerto rico", u"pr"},
         {u"rhode island", u"ri"},
         {u"south carolina", u"sc"},
         {u"south dakota", u"sd"},
         {u"tennessee", u"tn"},
         {u"texas", u"tx"},
         {u"utah", u"ut"},
         {u"vermont", u"vt"},
         {u"virginia", u"va"},
         {u"washington", u"wa"},
         {u"west virginia", u"wv"},
         {u"wisconsin", u"wi"},
         {u"wyoming", u"wy"}});

}  // namespace

std::u16string_view GetAbbreviationForName(std::u16string_view name) {
  auto it = kStateData.find(base::ToLowerASCII(name));
  return it != kStateData.end() ? it->second : std::u16string_view();
}

std::u16string_view GetNameForAbbreviation(std::u16string_view abbreviation) {
  using Member = decltype(kStateData)::value_type;
  auto it = base::ranges::find(kStateData, base::ToLowerASCII(abbreviation),
                               &Member::second);
  return it != kStateData.end() ? it->first : std::u16string_view();
}

void GetNameAndAbbreviation(std::u16string_view value,
                            std::u16string* name,
                            std::u16string* abbreviation) {
  std::u16string_view full = GetNameForAbbreviation(value);
  std::u16string_view abbr = value;
  if (full.empty()) {
    abbr = GetAbbreviationForName(value);
    full = value;
  }

  if (name) {
    *name = full;
  }
  if (abbreviation) {
    *abbreviation = abbr;
  }
}

}  // namespace autofill::state_names
