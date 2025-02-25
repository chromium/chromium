// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_AUTOFILL_STRUCTURED_ADDRESS_CONSTANTS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_AUTOFILL_STRUCTURED_ADDRESS_CONSTANTS_H_

namespace autofill {

// List of name separators.
extern const char kNameSeparators[];

// Regular expression pattern to match separators as used in CJK names:
// Included separators: \u002D, \u30FB, \u00B7, \u3000 or a simple space.
inline constexpr char kCjkNameSeparatorsRe[] = "(?:-|・|·|　|\\s+)";

}  // namespace autofill
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_ADDRESSES_AUTOFILL_STRUCTURED_ADDRESS_CONSTANTS_H_
