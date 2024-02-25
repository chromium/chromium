// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_FEATURES_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace web_app {

// Enables a split of the UserDisplayMode field in the web app database and
// sync, into CrOS and non-CrOS values. Turning it on also populates the field
// for the current platform at database load time (migrating from the existing
// non-CrOS field) to ensure it always has a value. Turning this off reverts to
// always using the old value (now named non-CrOS).
BASE_DECLARE_FEATURE(kSeparateUserDisplayModeForCrOS);

// Maintains the synced value of a new split UserDisplayMode field (see
// kSeparateUserDisplayModeForCrOS above), but doesn't actually use it. This
// prevents the field from being inadvertently cleared by the client.
BASE_DECLARE_FEATURE(kSyncOnlySeparateUserDisplayModeForCrOS);

#if BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kUserDisplayModeSyncBrowserMitigation);

BASE_DECLARE_FEATURE(kUserDisplayModeSyncStandaloneMitigation);
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_FEATURES_H_
