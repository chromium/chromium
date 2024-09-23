// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_FEATURES_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace web_app {

#if BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kUserDisplayModeSyncBrowserMitigation);

BASE_DECLARE_FEATURE(kUserDisplayModeSyncStandaloneMitigation);
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_FEATURES_H_
