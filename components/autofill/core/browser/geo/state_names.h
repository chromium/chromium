// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_STATE_NAMES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_STATE_NAMES_H_

#include "base/strings/string16.h"

namespace autofill {
namespace state_names {

// Returns the abbrevation corresponding to the state named |name|, or the empty
// string if there is no such state.
base::string16 GetAbbreviationForName(const base::string16& name);

// Returns the full state name corresponding to the |abbrevation|, or the empty
// string if there is no such state.
base::string16 GetNameForAbbreviation(const base::string16& abbreviation);

// |value| is either a state name or abbreviation. Detects which it is, and
// outputs both |name| and |abbreviation|. If it's neither, then |name| is
// set to |value| and |abbreviation| will be empty.
void GetNameAndAbbreviation(const base::string16& value,
                            base::string16* name,
                            base::string16* abbreviation);

}  // namespace state_names
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_STATE_NAMES_H_
