// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace toast_features {

// Enables the link copied confirmation toast.
BASE_FEATURE(kLinkCopiedToast, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the image copied confirmation toast.
BASE_FEATURE(kImageCopiedToast, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the video frame copied confirmation toast.
BASE_FEATURE(kVideoFrameCopiedToast, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the link to highlight copied confirmation toast.
BASE_FEATURE(kLinkToHighlightCopiedToast, base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled the page added to reading list confirmation toast.
BASE_FEATURE(kReadingListToast, base::FEATURE_ENABLED_BY_DEFAULT);

// Migrates the lens overlay toast to the toast framework.
BASE_FEATURE(kLensOverlayToast, base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled the clear browsing data confirmation toast.
BASE_FEATURE(kClearBrowsingDataToast, base::FEATURE_DISABLED_BY_DEFAULT);

// static
bool IsEnabled(const base::Feature& feature) {
  return base::FeatureList::IsEnabled(feature);
}

}  // namespace toast_features
