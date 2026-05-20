// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_finder.h"

#include <stdint.h>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/multi_user/multi_user_window_manager.h"
#include "ash/shell.h"
#include "base/check_is_test.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "components/account_id/account_id.h"
#endif

using content::WebContents;

namespace {

// See BrowserMatches for details.
const uint32_t kMatchOriginalProfile = 1 << 0;
const uint32_t kMatchCanSupportWindowFeature = 1 << 1;
const uint32_t kMatchNormal = 1 << 2;
const uint32_t kMatchDisplayId = 1 << 3;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
const uint32_t kMatchCurrentWorkspace = 1 << 4;
#endif
// If set, a Browser marked for deletion will be returned. Generally
// code using these functions does not want a browser scheduled for deletion,
// but there are outliers.
const uint32_t kIncludeBrowsersScheduledForDeletion = 1 << 5;

bool DoesBrowserMatchProfile(BrowserWindowInterface& browser,
                             const Profile* profile,
                             uint32_t match_types) {
  if (match_types & kMatchOriginalProfile) {
    if (browser.GetProfile()->GetOriginalProfile() !=
        profile->GetOriginalProfile()) {
      return false;
    }
  } else {
    if (browser.GetProfile() != profile) {
      return false;
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Get the profile on which the window is currently shown.
  // ash::Shell might be NULL under test scenario.
  // TODO(crbug.com/427889779): Consider to drop this check.
  if (ash::Shell::HasInstance()) {
    ash::MultiUserWindowManager* const multi_user_window_manager =
        ash::Shell::Get()->multi_user_window_manager();
    const AccountId& shown_account_id =
        multi_user_window_manager->GetUserPresentingWindow(
            browser.GetWindow()->GetNativeWindow());
    Profile* shown_profile =
        shown_account_id.is_valid()
            ? multi_user_util::GetProfileFromAccountId(shown_account_id)
            : nullptr;
    if (shown_profile &&
        shown_profile->GetOriginalProfile() != profile->GetOriginalProfile()) {
      return false;
    }
  } else {
    CHECK_IS_TEST();
  }
#endif

  return true;
}

// Returns true if the specified |browser| matches the specified arguments.
// |match_types| is a bitmask dictating what parameters to match:
// . If kMatchAnyProfile is true, the profile is not considered.
// . If it contains kMatchOriginalProfile then the original profile of the
//   browser must match |profile->GetOriginalProfile()|. This is used to match
//   incognito windows.
// . If it contains kMatchCanSupportWindowFeature
//   |CanSupportWindowFeature(window_feature)| must return true.
// . If it contains kMatchNormal, the browser must be a normal tabbed browser.
// . Browsers scheduled for deletion are ignored unless match_types contains
//   kIncludeBrowsersScheduledForDeletion explicitly.
bool BrowserMatches(BrowserWindowInterface* browser,
                    const Profile* profile,
                    Browser::WindowFeature window_feature,
                    uint32_t match_types,
                    int64_t display_id) {
  if ((match_types & kMatchCanSupportWindowFeature) &&
      !browser->GetBrowserForMigrationOnly()->CanSupportWindowFeature(
          window_feature)) {
    return false;
  }

  if (!DoesBrowserMatchProfile(*browser, profile, match_types)) {
    return false;
  }

  if ((match_types & kMatchNormal) &&
      browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return false;
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  // Note that |browser->window()| might be nullptr in tests.
  if ((match_types & kMatchCurrentWorkspace) &&
      (!browser->GetBrowserForMigrationOnly()->window() ||
       !browser->GetBrowserForMigrationOnly()
            ->window()
            ->IsOnCurrentWorkspace())) {
    return false;
  }
#endif

  if (match_types & kMatchDisplayId &&
      display::Screen::Get()
              ->GetDisplayNearestWindow(browser->GetWindow()->GetNativeWindow())
              .id() != display_id) {
    return false;
  }

  if ((match_types & kIncludeBrowsersScheduledForDeletion) == 0 &&
      browser->GetBrowserForMigrationOnly()->is_delete_scheduled()) {
    return false;
  }

  return true;
}

size_t GetBrowserCountImpl(Profile* profile,
                           uint32_t match_types,
                           int64_t display_id = display::kInvalidDisplayId) {
  size_t count = 0;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&count, profile, match_types,
       display_id](BrowserWindowInterface* browser) {
        if (BrowserMatches(browser, profile,
                           Browser::WindowFeature::kFeatureNone, match_types,
                           display_id)) {
          count++;
        }
        return true;
      });
  return count;
}

}  // namespace

namespace chrome {

Browser* FindBrowserWithTab(const WebContents* web_contents) {
  DCHECK(web_contents);
  Browser* found = nullptr;
  tabs::ForEachTabInterface([web_contents, &found](tabs::TabInterface* tab) {
    if (tab->GetContents() == web_contents) {
      found = tab->GetBrowserWindowInterface()->GetBrowserForMigrationOnly();
    }
    return !found;
  });
  return found;
}

size_t GetBrowserCount(Profile* profile) {
  return GetBrowserCountImpl(profile, kIncludeBrowsersScheduledForDeletion);
}

}  // namespace chrome
