// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/common/spelling_marker.h"

#include <stdint.h>

#include "components/spellcheck/common/spellcheck_decoration.h"

namespace spellcheck {

SpellingMarker::SpellingMarker(uint32_t start,
                               uint32_t end,
                               Decoration marker_type)
    : start(start), end(end), marker_type(marker_type) {}

SpellingMarker::SpellingMarker() = default;

SpellingMarker::SpellingMarker(const SpellingMarker&) = default;

SpellingMarker& SpellingMarker::operator=(const SpellingMarker&) = default;

SpellingMarker::~SpellingMarker() = default;

}  // namespace spellcheck
