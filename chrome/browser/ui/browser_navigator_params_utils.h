// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_NAVIGATOR_PARAMS_UTILS_H_
#define CHROME_BROWSER_UI_BROWSER_NAVIGATOR_PARAMS_UTILS_H_

#include <utility>

#include "content/public/browser/navigation_controller.h"

class BrowserWindowInterface;
class Profile;

namespace content {
class WebContents;
}  // namespace content

struct NavigateParams;

// Creates a content::NavigationController::LoadURLParams object from a
// NavigateParams object.
content::NavigationController::LoadURLParams LoadURLParamsFromNavigateParams(
    NavigateParams* params);

// Same as previous but sets navigation UI data for main frame navigations.
content::NavigationController::LoadURLParams LoadURLParamsFromNavigateParams(
    content::WebContents* target_contents,
    NavigateParams* params);

// If the given |params| specify a disposition of SINGLETON_TAB or
// SWITCH_TO_TAB, and the target URL is already open in |browser|,
// returns the index of the matching tab. Otherwise, returns -1.
// URL matching logic ignores the ref (hash). Depending on the
// |path_behavior| in |params|, it may also ignore the path and query.
// It matches view-source: scheme exactly.
int GetIndexOfExistingTabMatchingURL(BrowserWindowInterface* browser,
                                     const NavigateParams& params);

// This simply calls GetIndexOfExistingTabMatchingURL() for each browser that
// matches the passed |profile| and isn't scheduled for deletion.
// Returns the first found browser and matching tab index.
std::pair<BrowserWindowInterface*, int> GetIndexAndBrowserOfMatchingTab(
    Profile* profile,
    const NavigateParams& params);

#endif  // CHROME_BROWSER_UI_BROWSER_NAVIGATOR_PARAMS_UTILS_H_
