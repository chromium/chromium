// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_UTILS_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_UTILS_H_

#include "base/values.h"

namespace security_interstitials {

// Adjusts the interstitial page's template parameter "fontsize" by the font
// size multiplier.
void AdjustFontSize(base::Value::Dict& load_time_data,
                    float font_size_multiplier);

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_UTILS_H_
