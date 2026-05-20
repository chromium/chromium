// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_FINDER_H_
#define CHROME_BROWSER_UI_BROWSER_FINDER_H_

#include <stddef.h>

class Browser;
class Profile;

namespace content {
class WebContents;
}

// Collection of functions to find Browsers based on various criteria.
//
// WARNING: Most functions in this file should not be used.
//
// Functions in this file typically create non-local control flow. This is hard
// to stub for tests, hard to debug, and hard to maintain due to imprecise API
// surfaces. See
// https://source.chromium.org/chromium/chromium/src/+/main:docs/chrome_browser_design_principles.md
// for details. See TabInterface and TabFeatures for a common structure that
// avoids these functions.
//
// There are a few functions that have valid use cases. For example, we can
// imagine a hypothetical feature of the task manager (which is not conceptually
// associated with any given browser window) which wants to display the number
// of browser windows for a given profile to the user. This would be a valid
// use case for GetBrowserCount().
//
// More colloquial explanation:
// There are two problems with FindBrowser(*) methods.
//  (1) It returns a Browser*. Browser is a god object which is an antipattern.
//  We should try not to use it.
//  (2) Inverts the control flow. We typically want to pass state into the
//  objects that need it. e.g.
//  std::make_unique<ClassFoo>(dependency_that_foo_requires)
// So in general we shouldn't be looking up a Browser* from a WebContents*, we
// should be passing in the state we need, to the class that owns the
// WebContents*.
//
// BrowserWindowInterface is intended to be a replacement for Browser*. The
// reason this exists as opposed to just passing in the relevant state in the
// constructor of the class is because we have 2 important primitives in chrome
// that have different lifetimes (tab & window) and the relationship is dynamic
// (tabs can move between browser windows).
//
// Example 1: let's say that we have a feature that is conceptually tied to the
// window (e.g. tab search). UserEducation is also conceptually tied to the
// window. If we want to use UserEducation from tab search, we should pass in
// some type of pointer in the constructor of TabSearch, e.g.
// std::make_unique<TabSearch>(user_education).
//
// Example 2: let's say that we have a feature that is conceptually tied to the
// tab (e.g. find in page). In this case, we shouldn't pass a UserEducation to
// the tab feature, since it's possible for the tab to be dragged into a new
// window, and then we'd be pointing to the wrong UserEducation. So instead we
// should pass in a TabInterface (which will always be valid). Then anytime
// UserEducation is needed, dynamically fetch the feature via
// TabInterface->GetBrowserWindowInterface()->GetUserEducation()

namespace chrome {

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
// WARNING: Do not use this method. See comment at top of file.
Browser* FindBrowserWithTab(const content::WebContents* web_contents);

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

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_FINDER_H_
