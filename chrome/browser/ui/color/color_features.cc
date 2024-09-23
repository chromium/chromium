// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/color_features.h"

namespace features {

// Changes info bar icons to be monochrome instead using an accent color. Also
// adjusts the text color to be less subtle.
BASE_FEATURE(kInfoBarIconMonochrome,
             "InfoBarIconMonochrome",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
