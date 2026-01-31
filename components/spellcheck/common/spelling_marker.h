// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_COMMON_SPELLING_MARKER_H_
#define COMPONENTS_SPELLCHECK_COMMON_SPELLING_MARKER_H_

#include <stdint.h>

#include <compare>

#include "components/spellcheck/common/spellcheck_decoration.h"

namespace spellcheck {

// This class represents spelling markers, i.e. Spelling, Grammar.
struct SpellingMarker {
  SpellingMarker(uint32_t start, uint32_t end, Decoration marker_type);
  SpellingMarker();

  SpellingMarker(const SpellingMarker&);
  SpellingMarker& operator=(const SpellingMarker&);

  ~SpellingMarker();

  bool operator==(const SpellingMarker& other) const = default;

  uint32_t start;
  uint32_t end;
  Decoration marker_type;
};

}  // namespace spellcheck

#endif  // COMPONENTS_SPELLCHECK_COMMON_SPELLING_MARKER_H_
