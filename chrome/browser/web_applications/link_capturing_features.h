// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_LINK_CAPTURING_FEATURES_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_LINK_CAPTURING_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"

namespace apps::features {

// When enabled, updates the app settings string labels for browser-tab PWAs
// that support target-existing client modes (focus-existing or
// navigate-existing) to reflect that supported links can be opened in an
// existing app tab.
BASE_DECLARE_FEATURE(kUpdateAppStringsOnSettings);

// Flag that is used to override the "original" behavior from chrome://flags for
// testing. Requires the `kPwaNavigationCapturing` flag to NOT be explicitly
// disabled to work.
BASE_DECLARE_FEATURE(kPwaNavigationCapturingTestingOverride);
extern const base::FeatureParam<::features::CapturingState>
    kNavigationCapturingTestingOverrideState;

#if BUILDFLAG(IS_CHROMEOS)
// Kill switch, enabled by default. When enabled, the navigation capturing
// migration and the rollback respect the explicit user choice for "Opening
// supported links" that is recorded on the `WebApp`
// (`WebApp::user_link_capturing_preference()`). The choice is always recorded;
// this switch does not guard the write. It also guards these checks, which do
// not read the recorded choice:
// - The migration, and the install and update of web apps that are not IWAs
//   and not `ChromeOsWebAppExperiments` apps, check `CanCaptureLinksInScope()`
//   before they apply the default.
// - The migration does not replace a preferred web app with the same scope.
// - The rollback restores an app only if it is preferred before the rollback.
// Please see b/578641259 for more information.
BASE_DECLARE_FEATURE(kRespectUserLinkCapturingChoiceOnChromeOS);

// Returns true if the recorded user link capturing choice must be respected.
bool ShouldRespectRecordedLinkCapturingChoice();
#endif  // BUILDFLAG(IS_CHROMEOS)

// Returns true if the updated UX for link capturing needs to be shown. Only set
// to true on desktop platforms if kPwaNavigationCapturing is enabled, and
// always on CrOS.
bool ShouldShowLinkCapturingUX();

// Returns the active navigation capturing default state, checking
// kPwaNavigationCapturingTestingOverride first if enabled, otherwise falling
// back to kNavigationCapturingDefaultState.
::features::CapturingState GetNavigationCapturingDefaultState();

// Returns true if navigation capturing is on by default.
bool IsNavigationCapturingOnByDefault();

}  // namespace apps::features

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_LINK_CAPTURING_FEATURES_H_
