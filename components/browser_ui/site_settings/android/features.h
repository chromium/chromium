// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_SITE_SETTINGS_ANDROID_FEATURES_H_
#define COMPONENTS_BROWSER_UI_SITE_SETTINGS_ANDROID_FEATURES_H_

#include "base/feature_list.h"

namespace browser_ui {

// Improved 'All sites' and 'Site settings' pages on Android.
BASE_DECLARE_FEATURE(kSiteDataImprovements);
BASE_DECLARE_FEATURE(kRequestDesktopSiteExceptionsDowngrade);

}  // namespace browser_ui

#endif  // COMPONENTS_BROWSER_UI_SITE_SETTINGS_ANDROID_FEATURES_H_
