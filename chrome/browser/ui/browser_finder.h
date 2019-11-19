// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_FINDER_H_
#define CHROME_BROWSER_UI_BROWSER_FINDER_H_

#include <stddef.h>

#include "chrome/browser/ui/browser.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace contents {
class WebContents;
}

// Collection of functions to find Browsers based on various criteria.

namespace chrome {

// If you want to find the last active tabbed browser and create a new browser
// if there are no tabbed browsers, use ScopedTabbedBrowserDisplayer.

// Retrieve the last active tabbed browser with a profile matching |profile|.
// If |match_original_profiles| is true, matching is done based on the
// original profile, eg profile->GetOriginalProfile() ==
// browser->profile()->GetOriginalProfile(). This has the effect of matching
// against both non-incognito and incognito profiles. If
// |match_original_profiles| is false, only an exact match may be returned.
// If |display_id| is not equal to display::kInvalidDisplayId, only the browsers
// in the corresponding display may be returned.
Browser* FindTabbedBrowser(Profile* profile,
                           bool match_original_profiles,
                           int64_t display_id = display::kInvalidDisplayId);

// Finds an existing browser window of any kind.
Browser* FindAnyBrowser(Profile* profile, bool match_original_profiles);

// Find an existing browser window with the provided profile. Searches in the
// order of last activation. Only browsers that have been active can be
// returned. Returns NULL if no such browser currently exists.
Browser* FindBrowserWithProfile(Profile* profile);

// Find an existing browser with the provided ID. Returns NULL if no such
// browser currently exists.
Browser* FindBrowserWithID(SessionID desired_id);

// Find the browser represented by |window| or NULL if not found.
Browser* FindBrowserWithWindow(gfx::NativeWindow window);

// Find the browser with active window or NULL if not found.
Browser* FindBrowserWithActiveWindow();

// Find the browser containing |web_contents| or NULL if none is found.
// |web_contents| must not be NULL.
Browser* FindBrowserWithWebContents(const content::WebContents* web_contents);

// Returns the Browser object owned by |profile| whose window was most recently
// active. If no such Browsers exist, returns NULL.
//
// WARNING: this is NULL until a browser becomes active. If during startup
// a browser does not become active (perhaps the user launches Chrome, then
// clicks on another app before the first browser window appears) then this
// returns NULL.
// WARNING #2: this will always be NULL in unit tests run on the bots.
Browser* FindLastActiveWithProfile(Profile* profile);

// Returns the Browser object whose window was most recently active. If no such
// Browsers exist, returns NULL.
//
// WARNING: this is NULL until a browser becomes active. If during startup
// a browser does not become active (perhaps the user launches Chrome, then
// clicks on another app before the first browser window appears) then this
// returns NULL.
// WARNING #2: this will always be NULL in unit tests run on the bots.
Browser* FindLastActive();

// Returns the number of browsers across all profiles.
size_t GetTotalBrowserCount();

// Returns the number of browsers with the Profile |profile|.
size_t GetBrowserCount(Profile* profile);

// Returns the number of tabbed browsers with the Profile |profile|.
size_t GetTabbedBrowserCount(Profile* profile);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_FINDER_H_
