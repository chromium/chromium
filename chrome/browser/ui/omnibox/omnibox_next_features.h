// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_NEXT_FEATURES_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_NEXT_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace omnibox {

enum class AddContextButtonVariant {
  // No "Add Context" button.
  kNone = 0,
  // Variant 1.
  kBelowResults = 1,
  // Variant 2.
  kAboveResults = 2,
  // Variant 3.
  kInline = 3,
};

BASE_DECLARE_FEATURE(kWebUIOmniboxAimPopup);
extern const base::FeatureParam<AddContextButtonVariant>
    kWebUIOmniboxAimPopupAddContextButtonVariantParam;
BASE_DECLARE_FEATURE(kWebUIOmniboxFullPopup);
BASE_DECLARE_FEATURE(kWebUIOmniboxPopup);
BASE_DECLARE_FEATURE(kWebUIOmniboxPopupDebug);
extern const base::FeatureParam<bool> kWebUIOmniboxPopupDebugSxSParam;

}  // namespace omnibox

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_NEXT_FEATURES_H_
