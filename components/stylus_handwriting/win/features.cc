// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stylus_handwriting/win/features.h"

namespace stylus_handwriting::win {

BASE_FEATURE(kStylusHandwritingWin,
             "StylusHandwritingWin",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsStylusHandwritingWinEnabled() {
  return base::FeatureList::IsEnabled(
      ::stylus_handwriting::win::kStylusHandwritingWin);
}

}  // namespace stylus_handwriting::win
