// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_TEXT_FRAGMENTS_UTILS_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_TEXT_FRAGMENTS_UTILS_H_

#include <vector>

#include "url/gurl.h"

namespace shared_highlighting {

class TextFragment;

// Appends a set of text |fragments| with the correct format to the given
// |base_url|. Returns an empty GURL if |base_url| is invalid.
GURL AppendFragmentDirectives(const GURL& base_url,
                              std::vector<TextFragment> fragments);

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_TEXT_FRAGMENTS_UTILS_H_
