// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOASTS_TOAST_FEATURES_H_
#define CHROME_BROWSER_UI_TOASTS_TOAST_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// Base feature
BASE_DECLARE_FEATURE(kToastFramework);

// Individual toasts
BASE_DECLARE_FEATURE(kLinkCopiedToast);
BASE_DECLARE_FEATURE(kImageCopiedToast);
BASE_DECLARE_FEATURE(kLinkToHighlightCopiedToast);
BASE_DECLARE_FEATURE(kReadingListToast);
BASE_DECLARE_FEATURE(kLensOverlayToast);

}  // namespace features

#endif  // CHROME_BROWSER_UI_TOASTS_TOAST_FEATURES_H_
