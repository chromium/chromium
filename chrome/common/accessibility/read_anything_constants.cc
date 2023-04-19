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

// The font string to be used with gfx::FontList
// TODO(b/1266555): Replace all usages of gfx::FontList that use the font style
// string with the constructor that takes a vector of fonts instead.
const char kReadAnythingDefaultFontSyle[] = ", Sans-serif, 15px";

const char kLetterSpacingHistogramName[] =
    "Accessibility.ReadAnything.LetterSpacing";
const char kLineSpacingHistogramName[] =
    "Accessibility.ReadAnything.LineSpacing";
const char kColorHistogramName[] = "Accessibility.ReadAnything.Color";
const char kFontScaleHistogramName[] = "Accessibility.ReadAnything.FontScale";

}  // namespace string_constants
