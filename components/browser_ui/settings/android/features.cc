// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/settings/android/features.h"

#include "base/feature_list.h"

namespace browser_ui {

BASE_FEATURE(kHighlightManagedPrefDisclaimerAndroid,
             "HighlightManagedPrefDisclaimerAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace browser_ui
