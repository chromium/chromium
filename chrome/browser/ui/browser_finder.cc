// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_finder.h"

#include <stdint.h>

#include <algorithm>

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/base/interaction/element_identifier.h"
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

// Type used to indicate to match anything. This does not include browsers
// scheduled for deletion (see `kIncludeBrowsersScheduledForDeletion`).
const uint32_t kMatchAny = 0;

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
                             Profile* profile,
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
                    Profile* profile,
                    Browser::WindowFeature window_feature,
                    uint32_t match_types,
                    int64_t display_id) {
  if ((match_types & kMatchCanSupportWindowFeature) &&
      !browser->GetBrowserForMigrationOnly()->CanSupportWindowFeature(
          window_feature)) {
    return false;
  }

  if (!DoesBrowserMatchProfile(*browser->GetBrowserForMigrationOnly(), profile,
                               match_types)) {
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

// Returns the first BrowserWindowInterface that returns true from
// |BrowserMatches|, or null if no browsers match the arguments. See
// |BrowserMatches| for details on the arguments.
BrowserWindowInterface* FindBrowserOrderedByActivationMatching(
    Profile* profile,
    Browser::WindowFeature window_feature,
    uint32_t match_types,
    int64_t display_id = display::kInvalidDisplayId) {
  BrowserWindowInterface* match = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (BrowserMatches(browser, profile, window_feature, match_types,
                           display_id)) {
          match = browser;
          return false;  // stop iterating
        }
        return true;  // continue iterating
      });
  return match;
}

BrowserWindowInterface* FindBrowserWithTabbedOrAnyType(
    Profile* profile,
    bool match_tabbed,
    bool match_original_profiles,
    bool match_current_workspace,
    int64_t display_id = display::kInvalidDisplayId) {
  BrowserList* browser_list_impl = BrowserList::GetInstance();
  if (!browser_list_impl) {
    return nullptr;
  }
  uint32_t match_types = kMatchAny;
  if (match_tabbed) {
    match_types |= kMatchNormal;
  }
  if (match_original_profiles) {
    match_types |= kMatchOriginalProfile;
  }
  if (display_id != display::kInvalidDisplayId) {
    match_types |= kMatchDisplayId;
  }
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  if (match_current_workspace) {
    match_types |= kMatchCurrentWorkspace;
  }
#endif

  return FindBrowserOrderedByActivationMatching(
      profile, Browser::WindowFeature::kFeatureNone, match_types, display_id);
}

size_t GetBrowserCountImpl(Profile* profile,
                           uint32_t match_types,
                           int64_t display_id = display::kInvalidDisplayId) {
  BrowserList* browser_list_impl = BrowserList::GetInstance();
  size_t count = 0;
  if (browser_list_impl) {
    for (const auto& i : *browser_list_impl) {
      if (BrowserMatches(i, profile, Browser::WindowFeature::kFeatureNone,
                         match_types, display_id)) {
        count++;
      }
    }
  }
  return count;
}

}  // namespace

namespace chrome {

Browser* FindTabbedBrowser(Profile* profile,
                           bool match_original_profiles,
                           int64_t display_id) {
  BrowserWindowInterface* browser = FindBrowserWithTabbedOrAnyType(
      profile, true, match_original_profiles,
      /*match_current_workspace=*/true, display_id);
  return browser ? browser->GetBrowserForMigrationOnly() : nullptr;
}

Browser* FindAnyBrowser(Profile* profile, bool match_original_profiles) {
  BrowserWindowInterface* browser =
      FindBrowserWithTabbedOrAnyType(profile, false, match_original_profiles,
                                     /*match_current_workspace=*/false);
  return browser ? browser->GetBrowserForMigrationOnly() : nullptr;
}

Browser* FindBrowserWithProfile(Profile* profile) {
  BrowserWindowInterface* browser =
      FindBrowserWithTabbedOrAnyType(profile, false, false,
                                     /*match_current_workspace=*/false);
  return browser ? browser->GetBrowserForMigrationOnly() : nullptr;
}

std::vector<Browser*> FindAllTabbedBrowsersWithProfile(Profile* profile) {
  std::vector<Browser*> browsers;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (BrowserMatches(browser, profile,
                           Browser::WindowFeature::kFeatureNone, kMatchNormal,
                           display::kInvalidDisplayId)) {
          browsers.emplace_back(browser->GetBrowserForMigrationOnly());
        }
        return true;
      });
  return browsers;
}

std::vector<Browser*> FindAllBrowsersWithProfile(Profile* profile) {
  std::vector<Browser*> browsers;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (BrowserMatches(browser, profile,
                           Browser::WindowFeature::kFeatureNone, kMatchAny,
                           display::kInvalidDisplayId)) {
          browsers.emplace_back(browser->GetBrowserForMigrationOnly());
        }
        return true;
      });
  return browsers;
}

Browser* FindBrowserWithID(SessionID desired_id) {
  Browser* found = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetSessionID() == desired_id) {
          found = browser->GetBrowserForMigrationOnly();
        }
        return !found;
      });
  return found;
}

Browser* FindBrowserWithWindow(gfx::NativeWindow window) {
  if (!window) {
    return nullptr;
  }
  Browser* found = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetWindow() &&
            browser->GetWindow()->GetNativeWindow() == window) {
          found = browser->GetBrowserForMigrationOnly();
        }
        return !found;
      });
  return found;
}

Browser* FindBrowserWithActiveWindow() {
  BrowserWindowInterface* browser =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  return browser && browser->GetWindow()->IsActive()
             ? browser->GetBrowserForMigrationOnly()
             : nullptr;
}

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

Browser* FindBrowserWithGroup(tab_groups::TabGroupId group, Profile* profile) {
  Browser* found = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        TabStripModel* const tab_strip_model = browser->GetTabStripModel();
        if ((!profile || browser->GetProfile() == profile) && tab_strip_model &&
            tab_strip_model->group_model() &&
            tab_strip_model->group_model()->ContainsTabGroup(group)) {
          found = browser->GetBrowserForMigrationOnly();
        }
        return !found;
      });
  return found;
}

Browser* FindBrowserWithUiElementContext(ui::ElementContext context) {
  Browser* found = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (BrowserElements::From(browser)->GetContext() == context) {
          found = browser->GetBrowserForMigrationOnly();
        }
        return !found;
      });
  return found;
}

Browser* FindLastActiveWithProfile(Profile* profile) {
  // We are only interested in last active browsers, so we don't fall back to
  // all browsers like FindBrowserWith* do.
  BrowserWindowInterface* browser = FindBrowserOrderedByActivationMatching(
      profile, Browser::WindowFeature::kFeatureNone, kMatchAny);
  return browser ? browser->GetBrowserForMigrationOnly() : nullptr;
}

Browser* FindLastActive() {
  BrowserWindowInterface* last_active =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  return last_active ? last_active->GetBrowserForMigrationOnly() : nullptr;
}

size_t GetTotalBrowserCount() {
  size_t browser_count = 0;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        browser_count++;
        return true;
      });
  return browser_count;
}

size_t GetBrowserCount(Profile* profile) {
  return GetBrowserCountImpl(profile, kIncludeBrowsersScheduledForDeletion);
}

size_t GetTabbedBrowserCount(Profile* profile) {
  return GetBrowserCountImpl(
      profile, kMatchNormal | kIncludeBrowsersScheduledForDeletion);
}

}  // namespace chrome
