// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TOOLBAR_ADAPTIVE_TOOLBAR_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_TOOLBAR_ADAPTIVE_TOOLBAR_BRIDGE_H_

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/toolbar/adaptive_toolbar_enums.h"

namespace adaptive_toolbar {

void GetRankedSessionVariantButtons(
    Profile* profile,
    bool use_raw_results,
    base::OnceCallback<void(bool, std::vector<int>)> callback);

}  // namespace adaptive_toolbar

#endif  // CHROME_BROWSER_UI_ANDROID_TOOLBAR_ADAPTIVE_TOOLBAR_BRIDGE_H_
