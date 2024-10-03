// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace toast_features {

// Enables the new toast framework that allows features to trigger toasts. When
// this feature is disabled, no toasts will show.
BASE_FEATURE(kToastFramework,
             "ToastFramework",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kToastDemoMode{&kToastFramework,
                                              "toast_demo_mode", false};

const base::FeatureParam<base::TimeDelta> kToastTimeout{
    &kToastFramework, "toast_timeout", base::Seconds(10)};

const base::FeatureParam<base::TimeDelta> kToastWithoutActionTimeout{
    &kToastFramework, "toast_without_action_timeout", base::Seconds(10)};

// Enables the link copied confirmation toast.
BASE_FEATURE(kLinkCopiedToast,
             "LinkCopiedToast",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the image copied confirmation toast.
BASE_FEATURE(kImageCopiedToast,
             "ImageCopiedToast",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the link to highlight copied confirmation toast.
BASE_FEATURE(kLinkToHighlightCopiedToast,
             "LinkToHighlightCopiedToast",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled the page added to reading list confirmation toast.
BASE_FEATURE(kReadingListToast,
             "ReadingListToast",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Migrates the lens overlay toast to the toast framework.
BASE_FEATURE(kLensOverlayToast,
             "LensOverlayToast",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled the clear browsing data confirmation toast.
BASE_FEATURE(kClearBrowsingDataToast,
             "ClearBrowsingDataToast",
             base::FEATURE_DISABLED_BY_DEFAULT);

// static
bool IsEnabled(const base::Feature& feature) {
  return kToastDemoMode.Get() || base::FeatureList::IsEnabled(feature);
}

}  // namespace toast_features
