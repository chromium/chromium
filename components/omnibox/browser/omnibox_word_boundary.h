// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_WORD_BOUNDARY_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_WORD_BOUNDARY_H_

#include <stdint.h>

#include <string>

namespace omnibox {

// Returns the offset a word-granularity delete should extend to.
int32_t GetDeletionBoundary(const std::u16string& text,
                            int32_t cursor,
                            bool forward);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_WORD_BOUNDARY_H_
