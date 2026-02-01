// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_finder.h"

#include <stdint.h>

#include <algorithm>

#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
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
using ProfileBrowsersCloseCallback = chrome::ProfileBrowsersCloseCallback;

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

// Returns the first BrowserWindowInterface that returns true from
// |BrowserMatches|, or null if no browsers match the arguments. See
// |BrowserMatches| for details on the arguments.
BrowserWindowInterface* FindBrowserOrderedByActivationMatching(
    const Profile* profile,
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
    const Profile* profile,
    bool match_tabbed,
    bool match_original_profiles,
    bool match_current_workspace,
    int64_t display_id = display::kInvalidDisplayId) {
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

// Forward declaration.
void TryToCloseBrowsersForProfile(
    Profile* original_profile,
    bool match_original_profile,
    const ProfileBrowsersCloseCallback& on_close_success,
    const ProfileBrowsersCloseCallback& on_close_aborted,
    const base::FilePath& profile_path,
    bool skip_beforeunload);

void PostTryToCloseBrowsersForProfile(
    Profile* original_profile,
    bool match_original_profile,
    const ProfileBrowsersCloseCallback& on_close_success,
    const ProfileBrowsersCloseCallback& on_close_aborted,
    const base::FilePath& profile_path,
    bool skip_beforeunload,
    bool tab_close_confirmed) {
  static bool resetting_handlers = false;

  if (tab_close_confirmed) {
    TryToCloseBrowsersForProfile(original_profile, match_original_profile,
                                 on_close_success, on_close_aborted,
                                 profile_path, skip_beforeunload);
  } else if (!resetting_handlers) {
    base::AutoReset<bool> resetting_handlers_scoper(&resetting_handlers, true);
    GlobalBrowserCollection::GetInstance()->ForEach(
        [original_profile,
         match_original_profile](BrowserWindowInterface* browser) {
          bool matches = match_original_profile
                             ? browser->GetProfile()->GetOriginalProfile() ==
                                   original_profile
                             : browser->GetProfile() == original_profile;
          if (matches) {
            browser->GetBrowserForMigrationOnly()->ResetTryToCloseWindow();
          }
          return true;
        });
    if (on_close_aborted) {
      on_close_aborted.Run(profile_path);
    }
  }
}

void TryToCloseBrowsersForProfile(
    Profile* original_profile,
    bool match_original_profile,
    const ProfileBrowsersCloseCallback& on_close_success,
    const ProfileBrowsersCloseCallback& on_close_aborted,
    const base::FilePath& profile_path,
    bool skip_beforeunload) {
  auto matches_profile = [original_profile, match_original_profile](
                             BrowserWindowInterface* browser) {
    return match_original_profile
               ? browser->GetProfile()->GetOriginalProfile() == original_profile
               : browser->GetProfile() == original_profile;
  };

  bool waiting_for_close = false;

  GlobalBrowserCollection::GetInstance()->ForEach(
      [&](BrowserWindowInterface* browser) {
        if (!matches_profile(browser)) {
          return true;
        }
        if (browser->GetBrowserForMigrationOnly()->TryToCloseWindow(
                skip_beforeunload,
                base::BindRepeating(&PostTryToCloseBrowsersForProfile,
                                    original_profile, match_original_profile,
                                    on_close_success, on_close_aborted,
                                    profile_path, skip_beforeunload))) {
          waiting_for_close = true;
          return false;
        }
        return true;
      });

  if (waiting_for_close) {
    return;
  }

  if (on_close_success) {
    on_close_success.Run(profile_path);
  }

  GlobalBrowserCollection::GetInstance()->ForEach(
      [&](BrowserWindowInterface* browser) {
        if (matches_profile(browser) && browser->GetWindow()) {
          browser->GetWindow()->Close();
        }
        return true;
      });
}

}  // namespace

namespace chrome {

Browser* FindTabbedBrowser(const Profile* profile,
                           bool match_original_profiles,
                           int64_t display_id) {
  BrowserWindowInterface* browser = FindBrowserWithTabbedOrAnyType(
      profile, true, match_original_profiles,
      /*match_current_workspace=*/true, display_id);
  return browser ? browser->GetBrowserForMigrationOnly() : nullptr;
}

Browser* FindAnyBrowser(const Profile* profile, bool match_original_profiles) {
  BrowserWindowInterface* browser =
      FindBrowserWithTabbedOrAnyType(profile, false, match_original_profiles,
                                     /*match_current_workspace=*/false);
  return browser ? browser->GetBrowserForMigrationOnly() : nullptr;
}

Browser* FindBrowserWithProfile(const Profile* profile) {
  BrowserWindowInterface* browser =
      FindBrowserWithTabbedOrAnyType(profile, false, false,
                                     /*match_current_workspace=*/false);
  return browser ? browser->GetBrowserForMigrationOnly() : nullptr;
}

std::vector<Browser*> FindAllTabbedBrowsersWithProfile(const Profile* profile) {
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

std::vector<Browser*> FindAllBrowsersWithProfile(const Profile* profile) {
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

size_t GetIncognitoBrowserCount() {
  size_t incognito_browser_count = 0;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetProfile()->IsIncognitoProfile() &&
            browser->GetType() != BrowserWindowInterface::Type::TYPE_DEVTOOLS) {
          incognito_browser_count++;
        }
        return true;
      },
      BrowserCollection::Order::kActivation);
  return incognito_browser_count;
}

size_t GetTabbedBrowserCount(Profile* profile) {
  return GetBrowserCountImpl(
      profile, kMatchNormal | kIncludeBrowsersScheduledForDeletion);
}

void CloseAllBrowsersWithProfile(Profile* profile) {
  ProfileBrowserCollection* browser_collection =
      ProfileBrowserCollection::GetForProfile(profile);
  if (!browser_collection) {
    return;
  }

  browser_collection->ForEach(
      [profile](BrowserWindowInterface* browser) {
        if (browser->GetProfile()->GetOriginalProfile() ==
            profile->GetOriginalProfile()) {
          browser->GetWindow()->Close();
        }
        return true;
      });
}

size_t GetOffTheRecordBrowsersActiveForProfile(Profile* profile) {
  if (!profile) {
    return 0;
  }

  size_t incognito_window_count = 0;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [profile, &incognito_window_count](BrowserWindowInterface* browser) {
        if (browser->GetProfile()->IsSameOrParent(profile) &&
            browser->GetProfile()->IsOffTheRecord() &&
            browser->GetType() != BrowserWindowInterface::Type::TYPE_DEVTOOLS) {
          ++incognito_window_count;
        }
        return true;
      });
  return incognito_window_count;
}

bool IsOffTheRecordBrowserInUse(Profile* profile) {
  if (!profile) {
    return false;
  }

  bool off_the_record_in_use = false;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&](BrowserWindowInterface* browser) {
        Profile* window_profile = browser->GetProfile();
        if (window_profile && window_profile->IsSameOrParent(profile) &&
            window_profile->IsOffTheRecord()) {
          off_the_record_in_use = true;
        }
        return !off_the_record_in_use;
      });

  return off_the_record_in_use;
}

size_t GetGuestBrowserCount() {
  size_t guest_browser_count = 0;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&guest_browser_count](BrowserWindowInterface* browser) {
        if (browser->GetProfile()->IsGuestSession() &&
            browser->GetType() != BrowserWindowInterface::Type::TYPE_DEVTOOLS) {
          guest_browser_count++;
        }
        return true;
      });
  return guest_browser_count;
}

void CloseAllBrowsersWithProfile(
    Profile* profile,
    bool skip_beforeunload,
    const ProfileBrowsersCloseCallback& on_close_success,
    const ProfileBrowsersCloseCallback& on_close_aborted) {
  SessionServiceFactory::ShutdownForProfile(profile);
  AppSessionServiceFactory::ShutdownForProfile(profile);

  TryToCloseBrowsersForProfile(profile->GetOriginalProfile(),
                               /*match_original_profile=*/true,
                               on_close_success, on_close_aborted,
                               profile->GetPath(), skip_beforeunload);
}

void CloseAllBrowsersWithIncognitoProfile(Profile* profile,
                                          bool skip_beforeunload) {
  CHECK(profile->IsOffTheRecord());

  // If any matching browser is devtools, we can't skip beforeunload.
  if (skip_beforeunload) {
    GlobalBrowserCollection::GetInstance()->ForEach(
        [profile, &skip_beforeunload](BrowserWindowInterface* browser) {
          if (browser->GetProfile() == profile &&
              browser->GetType() ==
                  BrowserWindowInterface::Type::TYPE_DEVTOOLS) {
            skip_beforeunload = false;
            return false;
          }
          return true;
        });
  }

  TryToCloseBrowsersForProfile(profile, /*match_original_profile=*/false,
                               base::NullCallback(), base::NullCallback(),
                               profile->GetPath(), skip_beforeunload);
}

}  // namespace chrome
