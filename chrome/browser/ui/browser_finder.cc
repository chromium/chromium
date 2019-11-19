// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_finder.h"

#include <stdint.h>

#include <algorithm>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "components/account_id/account_id.h"
#endif

using content::WebContents;

namespace {

// Type used to indicate to match anything.
const uint32_t kMatchAny = 0;

// See BrowserMatches for details.
const uint32_t kMatchOriginalProfile = 1 << 0;
const uint32_t kMatchCanSupportWindowFeature = 1 << 1;
const uint32_t kMatchNormal = 1 << 2;
const uint32_t kMatchDisplayId = 1 << 3;
#if defined(OS_WIN) || defined(OS_CHROMEOS)
const uint32_t kMatchCurrentWorkspace = 1 << 4;
#endif

// Returns true if the specified |browser| matches the specified arguments.
// |match_types| is a bitmask dictating what parameters to match:
// . If it contains kMatchOriginalProfile then the original profile of the
//   browser must match |profile->GetOriginalProfile()|. This is used to match
//   incognito windows.
// . If it contains kMatchCanSupportWindowFeature
//   |CanSupportWindowFeature(window_feature)| must return true.
// . If it contains kMatchNormal, the browser must be a normal tabbed browser.
bool BrowserMatches(Browser* browser,
                    Profile* profile,
                    Browser::WindowFeature window_feature,
                    uint32_t match_types,
                    int64_t display_id) {
  if ((match_types & kMatchCanSupportWindowFeature) &&
      !browser->CanSupportWindowFeature(window_feature)) {
    return false;
  }

#if defined(OS_CHROMEOS)
  // Get the profile on which the window is currently shown.
  // MultiUserWindowManagerHelper might be NULL under test scenario.
  ash::MultiUserWindowManager* const multi_user_window_manager =
      MultiUserWindowManagerHelper::GetWindowManager();
  Profile* shown_profile = nullptr;
  if (multi_user_window_manager) {
    const AccountId& shown_account_id =
        multi_user_window_manager->GetUserPresentingWindow(
            browser->window()->GetNativeWindow());
    shown_profile =
        shown_account_id.is_valid()
            ? multi_user_util::GetProfileFromAccountId(shown_account_id)
            : nullptr;
  }
#endif

  if (match_types & kMatchOriginalProfile) {
    if (browser->profile()->GetOriginalProfile() !=
        profile->GetOriginalProfile())
      return false;
#if defined(OS_CHROMEOS)
    if (shown_profile &&
        shown_profile->GetOriginalProfile() != profile->GetOriginalProfile()) {
      return false;
    }
#endif
  } else {
    if (browser->profile() != profile)
      return false;
#if defined(OS_CHROMEOS)
    if (shown_profile && shown_profile != profile)
      return false;
#endif
  }

  if ((match_types & kMatchNormal) && !browser->is_type_normal())
    return false;

#if defined(OS_WIN) || defined(OS_CHROMEOS)
  // Note that |browser->window()| might be nullptr in tests.
  if ((match_types & kMatchCurrentWorkspace) &&
      (!browser->window() || !browser->window()->IsOnCurrentWorkspace())) {
    return false;
  }
#endif  // OS_WIN

  if (match_types & kMatchDisplayId) {
    return display::Screen::GetScreen()
               ->GetDisplayNearestWindow(browser->window()->GetNativeWindow())
               .id() == display_id;
  }

  return true;
}

// Returns the first browser in the specified iterator that returns true from
// |BrowserMatches|, or null if no browsers match the arguments. See
// |BrowserMatches| for details on the arguments.
template <class T>
Browser* FindBrowserMatching(const T& begin,
                             const T& end,
                             Profile* profile,
                             Browser::WindowFeature window_feature,
                             uint32_t match_types,
                             int64_t display_id = display::kInvalidDisplayId) {
  for (T i = begin; i != end; ++i) {
    if (BrowserMatches(*i, profile, window_feature, match_types, display_id))
      return *i;
  }
  return NULL;
}

Browser* FindBrowserWithTabbedOrAnyType(
    Profile* profile,
    bool match_tabbed,
    bool match_original_profiles,
    bool match_current_workspace,
    int64_t display_id = display::kInvalidDisplayId) {
  BrowserList* browser_list_impl = BrowserList::GetInstance();
  if (!browser_list_impl)
    return NULL;
  uint32_t match_types = kMatchAny;
  if (match_tabbed)
    match_types |= kMatchNormal;
  if (match_original_profiles)
    match_types |= kMatchOriginalProfile;
  if (display_id != display::kInvalidDisplayId)
    match_types |= kMatchDisplayId;
#if defined(OS_WIN) || defined(OS_CHROMEOS)
  if (match_current_workspace)
    match_types |= kMatchCurrentWorkspace;
#endif
  Browser* browser =
      FindBrowserMatching(browser_list_impl->begin_last_active(),
                          browser_list_impl->end_last_active(), profile,
                          Browser::FEATURE_NONE, match_types, display_id);
  // Fall back to a forward scan of all Browsers if no active one was found.
  return browser ? browser
                 : FindBrowserMatching(
                       browser_list_impl->begin(), browser_list_impl->end(),
                       profile, Browser::FEATURE_NONE, match_types, display_id);
}

size_t GetBrowserCountImpl(Profile* profile,
                           uint32_t match_types,
                           int64_t display_id = display::kInvalidDisplayId) {
  BrowserList* browser_list_impl = BrowserList::GetInstance();
  size_t count = 0;
  if (browser_list_impl) {
    for (auto i = browser_list_impl->begin(); i != browser_list_impl->end();
         ++i) {
      if (BrowserMatches(*i, profile, Browser::FEATURE_NONE, match_types,
                         display_id))
        count++;
    }
  }
  return count;
}

}  // namespace

namespace chrome {

Browser* FindTabbedBrowser(Profile* profile,
                           bool match_original_profiles,
                           int64_t display_id) {
  return FindBrowserWithTabbedOrAnyType(profile, true, match_original_profiles,
                                        /*match_current_workspace=*/true,
                                        display_id);
}

Browser* FindAnyBrowser(Profile* profile, bool match_original_profiles) {
  return FindBrowserWithTabbedOrAnyType(profile, false, match_original_profiles,
                                        /*match_current_workspace=*/false);
}

Browser* FindBrowserWithProfile(Profile* profile) {
  return FindBrowserWithTabbedOrAnyType(profile, false, false,
                                        /*match_current_workspace=*/false);
}

Browser* FindBrowserWithID(SessionID desired_id) {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->session_id() == desired_id)
      return browser;
  }
  return NULL;
}

Browser* FindBrowserWithWindow(gfx::NativeWindow window) {
  if (!window)
    return NULL;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->window() && browser->window()->GetNativeWindow() == window)
      return browser;
  }
  return NULL;
}

Browser* FindBrowserWithActiveWindow() {
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  return browser && browser->window()->IsActive() ? browser : nullptr;
}

Browser* FindBrowserWithWebContents(const WebContents* web_contents) {
  DCHECK(web_contents);
  auto& all_tabs = AllTabContentses();
  auto it = std::find(all_tabs.begin(), all_tabs.end(), web_contents);

  return (it == all_tabs.end()) ? nullptr : it.browser();
}

Browser* FindLastActiveWithProfile(Profile* profile) {
  BrowserList* list = BrowserList::GetInstance();
  // We are only interested in last active browsers, so we don't fall back to
  // all browsers like FindBrowserWith* do.
  return FindBrowserMatching(list->begin_last_active(), list->end_last_active(),
                             profile, Browser::FEATURE_NONE, kMatchAny);
}

Browser* FindLastActive() {
  BrowserList* browser_list_impl = BrowserList::GetInstance();
  if (browser_list_impl)
    return browser_list_impl->GetLastActive();
  return NULL;
}

size_t GetTotalBrowserCount() {
  return BrowserList::GetInstance()->size();
}

size_t GetBrowserCount(Profile* profile) {
  return GetBrowserCountImpl(profile, kMatchAny);
}

size_t GetTabbedBrowserCount(Profile* profile) {
  return GetBrowserCountImpl(profile, kMatchNormal);
}

}  // namespace chrome
