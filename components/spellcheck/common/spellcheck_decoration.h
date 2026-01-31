// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_DECORATION_H_
#define COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_DECORATION_H_

namespace spellcheck {

// LINT.IfChange(DecorationEnum)
enum Decoration {
  // Red underline for misspelled words.
  SPELLING,

  // Gray underline for correctly spelled words that are incorrectly used in
  // their context.
  GRAMMAR,
  LAST = GRAMMAR,
};
// LINT.ThenChange(/components/spellcheck/browser/android/java/src/org/chromium/components/spellcheck/SpellingMarker.java:SpellCheckDecoration)

}  // namespace spellcheck

#endif  // COMPONENTS_SPELLCHECK_COMMON_SPELLCHECK_DECORATION_H_
