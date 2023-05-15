// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_FINDER_H_
#define CHROME_BROWSER_UI_BROWSER_FINDER_H_

#include <stddef.h>
#include <vector>

#include "ui/display/types/display_constants.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class Profile;
class SessionID;

namespace content {
class WebContents;
}

namespace tab_groups {
class TabGroupId;
}

namespace ui {
class ElementContext;
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
// in the corresponding display may be returned. If |ignore_closing_browsers| is
// false, browsers that are in the closing state (i.e. browsers registered in
// |BrowserList::currently_closing_browsers_|) may be returned.
Browser* FindTabbedBrowser(Profile* profile,
                           bool match_original_profiles,
                           int64_t display_id = display::kInvalidDisplayId,
                           bool ignore_closing_browsers = false);

// Finds an existing browser window of any kind.
Browser* FindAnyBrowser(Profile* profile, bool match_original_profiles);

// Find an existing browser window with the provided profile. Searches in the
// order of last activation. Only browsers that have been active can be
// returned. Returns NULL if no such browser currently exists.
Browser* FindBrowserWithProfile(Profile* profile);

// Find all tabbed browsers with the provided profile. Returns an empty vector
// if no such browser currently exists.
std::vector<Browser*> FindAllTabbedBrowsersWithProfile(Profile* profile);

// Find all browsers of any type with the provided profile. Returns an empty
// vector if no such browser currently exists.
std::vector<Browser*> FindAllBrowsersWithProfile(Profile* profile);

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

// Find the browser containing the group with ID |group| or nullptr if none is
// found within the given |profile|. If the profile is not specified, find any
// browser containing the group.
Browser* FindBrowserWithGroup(tab_groups::TabGroupId group, Profile* profile);

// Find the browser for the given element context. Returns NULL if no such
// browser currently exists.
Browser* FindBrowserWithUiElementContext(ui::ElementContext context);

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
// Note that:
// 1. A profile may have non-browser windows. These are not counted.
// 2. A profile may have child profiles that have windows.  Those are not
//    counted. Thus, for example, a Guest profile (which is never displayed
//    directly) will return 0. (For a Guest profile, only the child off-the-
//    record profile is visible.)  Likewise, a parent profile with off-the-
//    record (Incognito) child profiles that have windows will not count those
//    child windows.
size_t GetBrowserCount(Profile* profile);

// Returns the number of tabbed browsers with the Profile |profile|.
size_t GetTabbedBrowserCount(Profile* profile);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_FINDER_H_
