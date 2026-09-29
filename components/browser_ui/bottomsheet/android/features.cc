// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/bottomsheet/android/features.h"

#include "base/feature_list.h"

namespace browser_ui {

// Kill switch for the fixes to how the bottom sheet hides one content and shows
// the next queued content.
BASE_FEATURE(kBottomSheetDeferContentSwapOnHidden,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBottomSheetTypes, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace browser_ui
