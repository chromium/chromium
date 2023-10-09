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

// Returns the last active tabbed browser with a profile matching `profile`.
//
// If `match_original_profiles` is true, matching is done based on the original
// profile (e.g. profile->GetOriginalProfile() ==
// browser->profile()->GetOriginalProfile()). This has the effect of matching
// against both non-incognito and incognito profiles. If
// `match_original_profiles` is false, only an exact match may be returned. If
// `display_id` is not equal to `display::kInvalidDisplayId`, only the browsers
// in the corresponding display may be returned. If `ignore_closing_browsers` is
// false, browsers that are in the closing state (i.e. browsers registered in
// `BrowserList::currently_closing_browsers_`) may be returned.
Browser* FindTabbedBrowser(Profile* profile,
                           bool match_original_profiles,
                           int64_t display_id = display::kInvalidDisplayId,
                           bool ignore_closing_browsers = false);

// Returns an existing browser window of any kind.
Browser* FindAnyBrowser(Profile* profile, bool match_original_profiles);

// Returns an existing browser window with the provided profile. Searches in the
// order of last activation. Only browsers that have been active can be
// returned. Returns nullptr if no such browser currently exists.
Browser* FindBrowserWithProfile(Profile* profile);

// Returns all tabbed browsers with the provided profile. Returns an empty
// vector if no such browsers currently exist.
std::vector<Browser*> FindAllTabbedBrowsersWithProfile(Profile* profile);

// Returns all browsers of any type with the provided profile. Returns an empty
// vector if no such browsers currently exist.
std::vector<Browser*> FindAllBrowsersWithProfile(Profile* profile);

// Returns an existing browser with the provided ID. Returns nullptr if no such
// browser currently exists.
Browser* FindBrowserWithID(SessionID desired_id);

// Returns the browser represented by `window`. Returns nullptr if no such
// browser currently exists.
Browser* FindBrowserWithWindow(gfx::NativeWindow window);

// Returns the browser with the currently active window. Returns nullptr if no
// such browser currently exists.
Browser* FindBrowserWithActiveWindow();

// Returns the browser containing the specified `web_contents` as a tab in that
// browser. Returns nullptr if no such browser currently exists. `web_contents`
// must not be nullptr.
//
// NOTE: Web App windows, Chrome App windows, popup windows, and other similar
// windows are implemented as browsers containing one tab, even though the tab
// strip is not displayed and the tab takes up the entire area of the window.
// Because of this implementation detail, this function will return such a
// browser if called for its contents.
//
// WARNING: This only will find a browser for which the specified contents is a
// tab. Other uses of WebContents within the browser will not cause the browser
// to be found via this method.
Browser* FindBrowserWithTab(const content::WebContents* web_contents);

// Returns the browser containing the group with ID `group` within the given
// `profile`. If the specified profile is nullptr, returns any browser
// containing a group with the given group ID. Returns nullptr if no such
// browser currently exists.
Browser* FindBrowserWithGroup(tab_groups::TabGroupId group, Profile* profile);

// Returns the browser for the given element context. Returns nullptr if no such
// browser currently exists.
Browser* FindBrowserWithUiElementContext(ui::ElementContext context);

// Returns the browser owned by `profile` whose window was most recently active.
// Returns nullptr if no such browser currently exists.
//
// WARNING: This returns nullptr until a browser becomes active. If during
// startup a browser does not become active (perhaps the user launches Chrome,
// then clicks on another app before the first browser window appears) then this
// returns nullptr.
//
// WARNING #2: This will always return nullptr in unit tests run on the bots.
Browser* FindLastActiveWithProfile(Profile* profile);

// Returns the browser whose window was most recently active. Returns nullptr if
// no such browser currently exists.
//
// WARNING: This returns nullptr until a browser becomes active. If during
// startup a browser does not become active (perhaps the user launches Chrome,
// then clicks on another app before the first browser window appears) then this
// returns nullptr.
//
// WARNING #2: This will always return nullptr in unit tests run on the bots.
Browser* FindLastActive();

// Returns the number of browsers across all profiles.
//
// WARNING: This function includes browsers scheduled for deletion whereas
// the majority of other functions do not.
size_t GetTotalBrowserCount();

// Returns the number of browsers with the Profile `profile`.
// Note that:
// 1. A profile may have non-browser windows. These are not counted.
// 2. A profile may have child profiles that have windows.  Those are not
//    counted. Thus, for example, a Guest profile (which is never displayed
//    directly) will return 0. (For a Guest profile, only the child off-the-
//    record profile is visible.)  Likewise, a parent profile with off-the-
//    record (Incognito) child profiles that have windows will not count those
//    child windows.
//
// WARNING: this function includes browsers scheduled for deletion whereas
// the majority of other functions do not.
size_t GetBrowserCount(Profile* profile);

// Returns the number of tabbed browsers with the Profile `profile`.
//
// WARNING: this function includes browsers scheduled for deletion whereas
// the majority of other functions do not.
size_t GetTabbedBrowserCount(Profile* profile);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_FINDER_H_
