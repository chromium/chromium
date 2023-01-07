// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/state_names.h"

#include <stddef.h>

#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace autofill {
namespace state_names {

namespace {

// TODO(jhawkins): Add more states/provinces.  See http://crbug.com/45039.

struct StateData {
  const char16_t* const name;
  const char16_t abbreviation[3];
};

const StateData kStateData[] = {
    {u"alabama", u"al"},
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
    {u"wyoming", u"wy"},
};

}  // namespace

std::u16string GetAbbreviationForName(const std::u16string& name) {
  auto* it = base::ranges::find(kStateData, base::ToLowerASCII(name),
                                &StateData::name);
  return it != std::end(kStateData) ? it->abbreviation : std::u16string();
}

std::u16string GetNameForAbbreviation(const std::u16string& abbreviation) {
  auto* it = base::ranges::find(kStateData, base::ToLowerASCII(abbreviation),
                                &StateData::abbreviation);
  return it != std::end(kStateData) ? it->name : std::u16string();
}

void GetNameAndAbbreviation(const std::u16string& value,
                            std::u16string* name,
                            std::u16string* abbreviation) {
  std::u16string full = GetNameForAbbreviation(value);
  std::u16string abbr = value;
  if (full.empty()) {
    abbr = GetAbbreviationForName(value);
    full = value;
  }

  if (name)
    name->swap(full);
  if (abbreviation)
    abbreviation->swap(abbr);
}

}  // namespace state_names
}  // namespace autofill
