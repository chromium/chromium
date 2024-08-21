// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_features.h"

namespace features {

// Enables the new toast framework that allows features to trigger toasts. When
// this feature is disabled, no toasts will show.
BASE_FEATURE(kToastFramework,
             "ToastFramework",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

}  // namespace features
