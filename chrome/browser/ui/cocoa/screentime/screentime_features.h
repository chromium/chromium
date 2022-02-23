// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SCREENTIME_SCREENTIME_FEATURES_H_
#define CHROME_BROWSER_UI_COCOA_SCREENTIME_SCREENTIME_FEATURES_H_

#include "base/feature_list.h"

namespace screentime {

extern const base::Feature kScreenTime;

bool IsScreenTimeEnabled();

}  // namespace screentime

#endif  // CHROME_BROWSER_UI_COCOA_SCREENTIME_SCREENTIME_FEATURES_H_
