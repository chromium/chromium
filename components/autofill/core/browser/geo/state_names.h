// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_STATE_NAMES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_STATE_NAMES_H_

#include <string>


namespace autofill {
namespace state_names {

// Returns the abbreviation corresponding to the state |name|, or the
// empty string if there is no such state.
std::u16string GetAbbreviationForName(const std::u16string& name);

// Returns the full state name corresponding to the |abbrevation|, or the empty
// string if there is no such state.
std::u16string GetNameForAbbreviation(const std::u16string& abbreviation);

// |value| is either a state name or abbreviation. Detects which it is, and
// outputs both |name| and |abbreviation|. If it's neither, then |name| is
// set to |value| and |abbreviation| will be empty.
void GetNameAndAbbreviation(const std::u16string& value,
                            std::u16string* name,
                            std::u16string* abbreviation);

}  // namespace state_names
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_STATE_NAMES_H_
