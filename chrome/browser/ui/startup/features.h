// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_FEATURES_H_
#define CHROME_BROWSER_UI_STARTUP_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// When enabled, the session storage database is destroyed and recreated on
// fresh non-recovery process startups where no session restore is needed.
BASE_DECLARE_FEATURE(kClearSessionStorageDiskStateOnStartup);

}  // namespace features

#endif  // CHROME_BROWSER_UI_STARTUP_FEATURES_H_
