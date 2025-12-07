// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_STATE_NAMES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_STATE_NAMES_H_

#include <string>
#include <string_view>

namespace autofill::state_names {

// Returns the abbreviation corresponding to the state `name`, or an empty
// string view if there is no such state.
std::u16string_view GetAbbreviationForName(std::u16string_view name);

// Returns the full state name corresponding to the `abbreviation`, or an empty
// string view if there is no such state.
std::u16string_view GetNameForAbbreviation(std::u16string_view abbreviation);

// `value` is either a state name or abbreviation. Detects which it is, and
// outputs both `name` and `abbreviation`. If it's neither, then `name` is
// set to `value` and `abbreviation` will be empty.
void GetNameAndAbbreviation(std::u16string_view value,
                            std::u16string* name,
                            std::u16string* abbreviation);

}  // namespace autofill::state_names

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_STATE_NAMES_H_
