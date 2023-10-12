// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UTIL_H_

#include "base/feature_list.h"
#include "base/functional/callback.h"
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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// The first value indicates that the logic for showing What's New is running.
// At most one of the remaining values will be logged for each run. If none of
// these values are logged, the page should try to load.
enum class StartupType {
  kCalledShouldShow = 0,
  kPromotionalTabsDisabled = 1,
  kInvalidState = 2,
  kFeatureDisabled = 3,
  kAlreadyShown = 4,
  kIneligible = 5,
  kOverridden = 6,
  kMaxValue = kOverridden,
};

// Exposed for testing.
BASE_DECLARE_FEATURE(kForceEnabled);

bool IsEnabled();

// Logs the type of startup (e.g. whether a user is eligible for What's New, and
// whether we try to show the page).
void LogStartupType(StartupType type);

// Disables loading remote content for tests, because this can lead to a
// redirect if it fails. Most tests don't expect redirects to occur.
void DisableRemoteContentForTests();

// Whether loading remote content has been disabled via
// DisableRemoteContentForTests().
bool IsRemoteContentDisabled();

// Whether the current CHROME_VERSION_MAJOR is a minimum of 117
bool IsMinimumRefreshVersion();

// Whether the current CHROME_VERSION_MAJOR is either 117 or 118
bool IsRefreshVersion();

// Allow setting the CHROME_VERSION_MAJOR for tests
void SetChromeVersionForTests(int chrome_version);

// Returns true if user has received the Chrome 2023 Refresh flag. Once
// the user/ has seen the Refresh version of the WNP, a pref is set to
// disable ever showing this page again.
bool ShouldShowRefresh(PrefService* local_state);

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
bool ShouldShowForState(PrefService* local_state,
                        bool promotional_tabs_enabled);

// Gets the server side URL for the What's New page for the current version of
// Chrome. If |may_redirect| is true, return a server URL that will redirect to
// the closest milestone page. Otherwise, return the direct URL of the current
// version, which may return 404 if there is no page for this milestone.
GURL GetServerURL(bool may_redirect);

// Same as GetServerURL, except version m117 and m118 are hard-coded to
// the same What's New version.
GURL GetServerURLForRefresh();

// Return the startup URL for the WebUI page.
GURL GetWebUIStartupURL();

// Starts fetching the What's New page and will open the page in |browser| if
// it exists.
void StartWhatsNewFetch(Browser* browser);

}  // namespace whats_new

#endif  // CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UTIL_H_
