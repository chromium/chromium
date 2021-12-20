// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UTIL_H_

#include "base/callback.h"
#include "url/gurl.h"

class Browser;
class PrefService;

namespace whats_new {
extern const char kChromeWhatsNewURL[];
extern const char kChromeWhatsNewURLShort[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LoadEvent {
  kLoadStart = 0,
  kLoadSuccess = 1,
  kLoadFailAndShowError = 2,
  kLoadFailAndFallbackToNtp = 3,
  kLoadFailAndCloseTab = 4,
  kLoadFailAndDoNotShow = 5,
  kMaxValue = kLoadFailAndDoNotShow,
};

// Disables loading remote content for tests, because this can lead to a
// redirect if it fails. Most tests don't expect redirects to occur.
void DisableRemoteContentForTests();

// Whether loading remote content has been disabled via
// DisableRemoteContentForTests().
bool IsRemoteContentDisabled();

// Returns true if the user has not yet seen the What's New page for the
// current major milestone. When returning true, sets the pref in |local_state|
// to indicate that What's New should not try to display again for the current
// major milestone.
// Note that this does not guarantee that the page will always show (for
// example, onboarding tabs override What's New, or remote content can fail to
// load, which will result in the tab not opening). However, What's New should
// only display automatically on the first relaunch after updating to a new
// major milestone, and it is preferable to only attempt to show the page once
// and possibly miss some users instead of repeatedly triggering a network
// request at startup and/or showing the same What's New page many times for a
// given user.
bool ShouldShowForState(PrefService* local_state);

// Gets the server side URL for the What's New page for the current version of
// Chrome. If |may_redirect| is true, return a server URL that will redirect to
// the closest milestone page. Otherwise, return the direct URL of the current
// version, which may return 404 if there is no page for this milestone.
GURL GetServerURL(bool may_redirect);

// Return the startup URL for the WebUI page.
GURL GetWebUIStartupURL();

// Starts fetching the What's New page and will open the page in |browser| if
// it exists.
void StartWhatsNewFetch(Browser* browser);

}  // namespace whats_new

#endif  // CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UTIL_H_
