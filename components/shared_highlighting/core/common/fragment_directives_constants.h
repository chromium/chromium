// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_FRAGMENT_DIRECTIVES_CONSTANTS_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_FRAGMENT_DIRECTIVES_CONSTANTS_H_

#include <string.h>

namespace shared_highlighting {

// Delimiter indicating the start of an anchor in a URL.
extern const char kAnchorDelimiter;

// Delimiter indicating the start of the text fragments in a URL.
extern const char kFragmentsUrlDelimiter[];
extern const int kFragmentsUrlDelimiterLength;

// Parameter name for a single text fragment directive in a URL.
extern const char kTextDirectiveParameterName[];
extern const size_t kTextDirectiveParameterNameLength;

// Parameter name for a single selector fragment directive in a URL.
extern const char kSelectorDirectiveParameterName[];
extern const int kSelectorDirectiveParameterNameLength;

extern const char kSelectorDirectiveSuffix[];
extern const int kSelectorDirectiveSuffixLength;

extern const char kSelectorDirectiveValuePrefix[];
extern const int kSelectorDirectiveValuePrefixLength;

extern const char kSelectorDirectiveTypePrefix[];
extern const int kSelectorDirectiveTypePrefixLength;

extern const char kSelectorJoinDelimeter[];
extern const int kSelectorJoinDelimeterLength;

extern const char kTypeCssSelector[];

// These values correspond to the keys used to store text fragment's values
// in a dictionary Value.
extern const char kFragmentPrefixKey[];
extern const char kFragmentTextStartKey[];
extern const char kFragmentTextEndKey[];
extern const char kFragmentSuffixKey[];

// Default highlight color stored as a hexadecimal number.
extern const int kFragmentTextBackgroundColorARGB;

// Default text color stored as a hexadecimal number.
extern const int kFragmentTextForegroundColorARGB;

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_FRAGMENT_DIRECTIVES_CONSTANTS_H_
