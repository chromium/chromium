// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_REGEXES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_REGEXES_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"

// Parsing utilities.
namespace autofill {

// Case-insensitive regular expression matching.
// Returns true if |pattern| is found in |input|.
// If |groups| is non-null, it gets resized and the found capture groups
// are written into it.
bool MatchesPattern(const base::StringPiece16& input,
                    const base::StringPiece16& pattern,
                    std::vector<std::u16string>* groups = nullptr);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_REGEXES_H_
