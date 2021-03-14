// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_REGEXES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_REGEXES_H_

#include <string>

#include "base/strings/string_piece.h"

// Parsing utilities.
namespace autofill {

// Case-insensitive regular expression matching.
// Returns true if |pattern| is found in |input|.
// The |group_to_be_captured| numbered group is captured into |match|.
bool MatchesPattern(const base::StringPiece16& input,
                    const base::StringPiece16& pattern,
                    std::u16string* match = nullptr,
                    int32_t group_to_be_captured = 0);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_REGEXES_H_
