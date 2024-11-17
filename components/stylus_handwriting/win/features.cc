// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stylus_handwriting/win/features.h"

namespace stylus_handwriting::win {

BASE_FEATURE(kStylusHandwritingWin,
             "StylusHandwritingWin",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsStylusHandwritingWinEnabled() {
  return base::FeatureList::IsEnabled(kStylusHandwritingWin);
}

BASE_FEATURE(kProximateBoundsCollection,
             "ProximateBoundsCollection",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kProximateBoundsCollectionHalfLimit,
                   &kProximateBoundsCollection,
                   "half_limit",
                   100);

uint32_t ProximateBoundsCollectionHalfLimit() {
  return static_cast<uint32_t>(
      std::abs(kProximateBoundsCollectionHalfLimit.Get()));
}

}  // namespace stylus_handwriting::win
