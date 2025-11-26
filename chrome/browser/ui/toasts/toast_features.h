// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOASTS_TOAST_FEATURES_H_
#define CHROME_BROWSER_UI_TOASTS_TOAST_FEATURES_H_

#include "base/feature_list.h"
#include "base/time/time.h"

namespace toast_features {

// Individual toasts
BASE_DECLARE_FEATURE(kLinkCopiedToast);
BASE_DECLARE_FEATURE(kImageCopiedToast);
BASE_DECLARE_FEATURE(kVideoFrameCopiedToast);
BASE_DECLARE_FEATURE(kLinkToHighlightCopiedToast);
BASE_DECLARE_FEATURE(kReadingListToast);
BASE_DECLARE_FEATURE(kLensOverlayToast);
BASE_DECLARE_FEATURE(kClearBrowsingDataToast);

// Wrapper function used to check if a specific toast feature is enabled. Must
// be used for toasts that are part of demo mode.
bool IsEnabled(const base::Feature& feature);

}  // namespace toast_features

#endif  // CHROME_BROWSER_UI_TOASTS_TOAST_FEATURES_H_
