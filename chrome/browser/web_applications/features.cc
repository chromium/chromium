// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/features.h"

namespace web_app {

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kUserDisplayModeSyncBrowserMitigation,
             "UserDisplayModeSyncBrowserMitigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUserDisplayModeSyncStandaloneMitigation,
             "UserDisplayModeSyncStandaloneMitigation",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app
