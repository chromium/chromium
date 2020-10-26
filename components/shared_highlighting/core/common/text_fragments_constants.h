// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_TEXT_FRAGMENTS_CONSTANTS_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_TEXT_FRAGMENTS_CONSTANTS_H_

namespace shared_highlighting {

// Delimiter indicating the start of the text fragments in a URL.
extern const char kFragmentsUrlDelimiter[];

// Parameter name for a single text fragment in a URL.
extern const char kFragmentParameterName[];

// These values correspond to the keys used to store text fragment's values
// in a dictionary Value.
extern const char kFragmentPrefixKey[];
extern const char kFragmentTextStartKey[];
extern const char kFragmentTextEndKey[];
extern const char kFragmentSuffixKey[];

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_TEXT_FRAGMENTS_CONSTANTS_H_
