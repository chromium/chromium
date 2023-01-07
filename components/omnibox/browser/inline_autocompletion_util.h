// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_INLINE_AUTOCOMPLETION_UTIL_H_
#define COMPONENTS_OMNIBOX_BROWSER_INLINE_AUTOCOMPLETION_UTIL_H_

#include <stddef.h>

#include <string>
#include <vector>
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "ui/gfx/range/range.h"

// Finds the first occurrence of |search| at a wordbreak within |text| starting
// at |search_start|.
size_t FindAtWordbreak(const std::u16string& text,
                       const std::u16string& search,
                       size_t search_start = 0);

#endif  // COMPONENTS_OMNIBOX_BROWSER_INLINE_AUTOCOMPLETION_UTIL_H_
