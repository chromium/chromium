// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/read_anything/read_anything_constants.h"

namespace string_constants {

// Used as an initial value in the model. This is not shown to the user.
// The font option shown to the user before a selection occurs is either from
// their saved preference or from the default selected_index_ in the font model.
const char kReadAnythingPlaceholderFontName[] = "Poppins";

// Used as a fallback font for css if the current font is unavailable or
// invalid.
const char kReadAnythingDefaultFont[] = "sans-serif";

}  // namespace string_constants
