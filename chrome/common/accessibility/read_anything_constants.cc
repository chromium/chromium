// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/accessibility/read_anything_constants.h"
#include "read_anything_constants.h"

namespace string_constants {

// The default Read Anything font that will be used before a selection is made.
// This will also be used as a backup font if another Read Anything font is
// unavailable by the system.
const char kReadAnythingDefaultFontName[] = "Sans-serif";

const char kLetterSpacingHistogramName[] =
    "Accessibility.ReadAnything.LetterSpacing";
const char kLineSpacingHistogramName[] =
    "Accessibility.ReadAnything.LineSpacing";
const char kColorHistogramName[] = "Accessibility.ReadAnything.Color";
const char kFontScaleHistogramName[] = "Accessibility.ReadAnything.FontScale";

}  // namespace string_constants
