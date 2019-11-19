// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STRING_CONVERSIONS_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STRING_CONVERSIONS_UTIL_H_

#include <string>
#include <vector>

#include "third_party/icu/source/common/unicode/umachine.h"

namespace autofill_assistant {

// Converts a UTF-8 encoded string to a vector of unicode codepoints.
std::vector<UChar32> UTF8ToUnicode(const std::string& text);

// Converts a series of unicode codepoints to an UTF-8 encoded string and
// assigns the result to |target|.
bool UnicodeToUTF8(const std::vector<UChar32>& source, std::string* target);

// Converts a single unicode codepoint to UTF-8 and appends the result to
// |target|.
bool AppendUnicodeToUTF8(const UChar32 source, std::string* target);

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STRING_CONVERSIONS_UTIL_H_
