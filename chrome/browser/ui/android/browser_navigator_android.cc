// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator.h"

#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_navigator_params_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/window_open_disposition.h"

namespace {

// Returns true if NavigateParams are valid, false otherwise.
bool ValidNavigateParams(NavigateParams* params) {
  // TODO (crbug.com/441594986) Confirm this is correct.
  DCHECK(params->browser);
  DCHECK(!params->contents_to_insert);
  DCHECK(!params->switch_to_singleton_tab);

  if (!params->initiating_profile) {
    params->initiating_profile = params->browser->GetProfile();
  }
  DCHECK(params->initiating_profile);

  if (params->initiating_profile->ShutdownStarted()) {
    // Don't navigate when the profile is shutting down.
    return false;
  }

  // If OFF_THE_RECORD disposition does not require a new window,
  // convert it into NEW_FOREGROUND_TAB.
  if (params->disposition == WindowOpenDisposition::OFF_THE_RECORD &&
      params->initiating_profile->IsOffTheRecord()) {
    params->disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  }

  return true;
}

// Helper to create/locate windows.
void GetOrCreateBrowserWindowForDisposition(
    NavigateParams* params,
    base::OnceCallback<void(BrowserWindowInterface*)> callback) {
  raw_ptr<Profile> profile = params->initiating_profile;
  switch (params->disposition) {
    case WindowOpenDisposition::OFF_THE_RECORD:
      // The existing profile was already checked and is not OTR
      // so we get an OTR profile and create a new window.
      profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
      [[fallthrough]];
    case WindowOpenDisposition::NEW_WINDOW: {
      BrowserWindowCreateParams create_params(*profile, params->user_gesture);
      CreateBrowserWindow(std::move(create_params), std::move(callback));
      break;
    }
    case WindowOpenDisposition::NEW_POPUP: {
      BrowserWindowCreateParams create_params(
          BrowserWindowInterface::Type::TYPE_POPUP, *profile,
          params->user_gesture);
      CreateBrowserWindow(std::move(create_params), std::move(callback));
      break;
    }
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      [[fallthrough]];
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      [[fallthrough]];
    case WindowOpenDisposition::CURRENT_TAB: {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), params->browser));
      break;
    }
    default:
      NOTIMPLEMENTED();
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), nullptr));
  }
}

// Helper to create/locate tabs.
raw_ptr<tabs::TabInterface> GetOrCreateTabForDisposition(
    BrowserWindowInterface* bwi,
    NavigateParams* params) {
  TabListInterface* tab_list = TabListInterface::From(bwi);
  if (!tab_list) {
    return nullptr;
  }
  switch (params->disposition) {
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      [[fallthrough]];
    case WindowOpenDisposition::NEW_FOREGROUND_TAB: {
      // Determine the insertion index.
      // If there's no active tab (e.g., empty tab list), insert at the
      // beginning. Otherwise if inserting a foreground tab, insert after the
      // active tab. Else insert background tab at end of list.
      // TODO (crbug.com/449738150) Match WML logic in
      // TabStripModel::DetermineInsertionIndex.
      int active_index = tab_list->GetActiveIndex();
      int insertion_index =
          active_index == -1 ? 0
          : params->disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB
              ? -1
              : active_index + 1;

      // Create a new tab (opens in the background).
      // TODO (crbug.com/449738150) Add way to get this NavigationHandle.
      raw_ptr<tabs::TabInterface> new_tab =
          tab_list->OpenTab(params->url, insertion_index);
      if (!new_tab || !new_tab->GetContents()) {
        return nullptr;
      }

      // Bring the new tab to the foreground if necessary.
      if (params->disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB) {
        tabs::TabHandle new_tab_handle = new_tab->GetHandle();
        tab_list->HighlightTabs(new_tab_handle, {new_tab_handle});
      }

      // The new tab's WebContents is the target for our navigation.
      params->source_contents = new_tab->GetContents();
      return new_tab;
    }
    case WindowOpenDisposition::CURRENT_TAB:
      if (params->source_contents) {
        return tabs::TabInterface::GetFromContents(params->source_contents);
      }
      // Otherwise use the active tab.
      [[fallthrough]];
    case WindowOpenDisposition::OFF_THE_RECORD:
      // A new incognito window has already been created with a new tab.
      [[fallthrough]];
    case WindowOpenDisposition::NEW_POPUP:
      [[fallthrough]];
    case WindowOpenDisposition::NEW_WINDOW: {
      // A new tab is already created when the new window is created on Android.
      // Just get the active tab.
      raw_ptr<tabs::TabInterface> active_tab = tab_list->GetActiveTab();
      params->source_contents = active_tab->GetContents();
      return active_tab;
    }
    default:
      NOTIMPLEMENTED();
      return nullptr;
  }
}

base::WeakPtr<content::NavigationHandle> PerformNavigation(
    raw_ptr<tabs::TabInterface> tab,
    NavigateParams* params) {
  if (!tab || !params->source_contents) {
    return nullptr;
  }
  content::WebContents* contents = tab->GetContents();
  params->navigated_or_inserted_contents = contents;
  // Perform the actual navigation on the determined source_contents.
  content::NavigationController::LoadURLParams load_url_params =
      LoadURLParamsFromNavigateParams(params);
  return contents->GetController().LoadURLWithParams(load_url_params);
}

void GetTabAndPerformNavigation(
    NavigateParams* params,
    base::OnceCallback<void(base::WeakPtr<content::NavigationHandle>)> callback,
    BrowserWindowInterface* bwi) {
  if (!bwi) {
    // If no browser window is available, the navigation cannot proceed.
    // The callback is run with nullptr to signal that the navigation was
    // aborted.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr));
    return;
  }
  tabs::TabInterface* tab = GetOrCreateTabForDisposition(bwi, params);
  base::WeakPtr<content::NavigationHandle> handle =
      PerformNavigation(tab, params);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), handle));
}

}  // end namespace

base::WeakPtr<content::NavigationHandle> Navigate(NavigateParams* params) {
  if (!ValidNavigateParams(params)) {
    return nullptr;
  }
  // Only handles dispositions that do not create new windows.
  if (params->disposition != WindowOpenDisposition::CURRENT_TAB &&
      params->disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB &&
      params->disposition != WindowOpenDisposition::NEW_FOREGROUND_TAB) {
    return nullptr;
  }
  auto tab = GetOrCreateTabForDisposition(params->browser, params);

  return PerformNavigation(tab, params);
}

void Navigate(NavigateParams* params,
              base::OnceCallback<void(base::WeakPtr<content::NavigationHandle>)>
                  callback) {
  if (!ValidNavigateParams(params)) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr));
    return;
  }
  GetOrCreateBrowserWindowForDisposition(
      params,
      base::BindOnce(&GetTabAndPerformNavigation, params, std::move(callback)));
}
