// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SINGLETON_TABS_H_
#define CHROME_BROWSER_UI_SINGLETON_TABS_H_

#include "chrome/browser/ui/browser_navigator_params.h"

class BrowserWindowInterface;
class GURL;

// Methods for opening "singleton tabs". Tabs are guaranteed unique by varying
// metrics within a particular Browser window.

// Core singleton tab API:

// Shows a given a URL. If a tab with the same URL (ignoring the ref) is already
// visible in this browser, it becomes selected. Otherwise a new tab is created.
void ShowSingletonTab(BrowserWindowInterface* browser, const GURL& url);

// Like above, but uses the last active tabbed browser or creates a new one if
// possible.
void ShowSingletonTab(Profile* profile, const GURL& url);

// Like ShowSingletonTab, but if the current tab is the new tab page or
// about:blank, then overwrite it with the passed contents.
void ShowSingletonTabOverwritingNTP(
    Profile* profile,
    const GURL& url,
    NavigateParams::PathBehavior path_behavior = NavigateParams::RESPECT);
void ShowSingletonTabOverwritingNTP(
    BrowserWindowInterface* browser,
    const GURL& url,
    NavigateParams::PathBehavior path_behavior = NavigateParams::RESPECT);
void ShowSingletonTabOverwritingNTP(NavigateParams* params);

// Creates a NavigateParams struct for a singleton tab navigation.
NavigateParams GetSingletonTabNavigateParams(BrowserWindowInterface* browser,
                                             const GURL& url);

#endif  // CHROME_BROWSER_UI_SINGLETON_TABS_H_
