// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_word_boundary.h"

#include <stdint.h>

#include <algorithm>
#include <string>

#include "base/android/jni_string.h"
#include "base/i18n/break_iterator.h"
#include "third_party/jni_zero/default_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/omnibox/browser/scheme_classifier_jni/OmniboxWordBoundary_jni.h"

namespace omnibox {

int32_t GetDeletionBoundary(const std::u16string& text,
                            int32_t cursor,
                            bool forward) {
  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
  if (cursor < 0 || !iter.Init()) {
    return cursor;
  }

  const size_t length = text.length();
  const size_t position = std::min(static_cast<size_t>(cursor), length);

  if (forward) {
    // Stop at the end of the next word, or the end of the text.
    for (size_t i = position + 1; i <= length; ++i) {
      if (i == length || iter.IsEndOfWord(i)) {
        return static_cast<int32_t>(i);
      }
    }
    return static_cast<int32_t>(length);
  }

  // Stop at the start of the previous word, or the beginning of the text.
  for (size_t i = position; i-- > 0;) {
    if (i == 0 || iter.IsStartOfWord(i)) {
      return static_cast<int32_t>(i);
    }
  }
  return 0;
}

}  // namespace omnibox

static int32_t JNI_OmniboxWordBoundary_GetDeletionBoundary(
    const std::u16string& text,
    int32_t cursor,
    bool forward) {
  return omnibox::GetDeletionBoundary(text, cursor, forward);
}

DEFINE_JNI(OmniboxWordBoundary)
