// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator.h"

#include "base/memory/raw_ptr.h"
#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_navigator_params_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
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

// Helper called in GetOrCreateTabForDisposition().
// Maps the NavigateParams to the appropriate Android TabLaunchType.
TabModel::TabLaunchType GetTabLaunchType(const NavigateParams* params) {
  using TabLaunchType = TabModel::TabLaunchType;

  // 1. Explicit Index:
  // If an explicit index is requested, use FROM_CHROME_UI. This type does NOT
  // trigger "adjacency" logic in Java, allowing the passed index to be
  // respected.
  if (params->tabstrip_index != -1) {
    return TabLaunchType::FROM_CHROME_UI;
  }

  bool is_background =
      params->disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB;
  bool is_link = ui::PageTransitionCoreTypeIs(params->transition,
                                              ui::PAGE_TRANSITION_LINK);

  // 2. Background Navigation:
  if (is_background) {
    if (is_link) {
      // Background Link (e.g., Ctrl+Click).
      // Use FROM_LONGPRESS_BACKGROUND to trigger "Group with Parent" logic
      // while keeping the tab in the background.
      return TabLaunchType::FROM_LONGPRESS_BACKGROUND;
    }
    // Generic Background (e.g., Middle-click Bookmark).
    // Use FROM_BROWSER_ACTIONS. In Java, this forces the tab to the END
    // of the model (ignoring adjacency) and keeps it backgrounded.
    return TabLaunchType::FROM_BROWSER_ACTIONS;
  }

  // 3. Foreground Navigation:
  if (is_link) {
    // Foreground Link.
    // Use FROM_LINK to trigger "Adjacent to Parent" logic and selection.
    return TabLaunchType::FROM_LINK;
  }

  if (ui::PageTransitionCoreTypeIs(params->transition,
                                   ui::PAGE_TRANSITION_TYPED) ||
      ui::PageTransitionCoreTypeIs(params->transition,
                                   ui::PAGE_TRANSITION_GENERATED)) {
    // Omnibox Navigation.
    // Use FROM_OMNIBOX to disable adjacency logic (placing it at the end).
    return TabLaunchType::FROM_OMNIBOX;
  }

  // Default Generic Foreground.
  return TabLaunchType::FROM_CHROME_UI;
}

// Helper to create/locate tabs.
raw_ptr<tabs::TabInterface> GetOrCreateTabForDisposition(
    BrowserWindowInterface* bwi,
    NavigateParams* params) {
  // On Android, TabListInterface is TabModel.
  TabModel* tab_model = static_cast<TabModel*>(TabListInterface::From(bwi));
  if (!tab_model) {
    return nullptr;
  }

  switch (params->disposition) {
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      [[fallthrough]];
    case WindowOpenDisposition::NEW_FOREGROUND_TAB: {
      // Select the TabLaunchType.
      TabModel::TabLaunchType launch_type = GetTabLaunchType(params);

      // Identify parent tab.
      // Parent tab is intentionally left as nullptr if the
      // TabLaunchType == FROM_OMNIBOX to ensure the tab is added as the last
      // tab (mirroring WML behavior).
      TabAndroid* parent = nullptr;
      if (params->source_contents &&
          launch_type != TabModel::TabLaunchType::FROM_OMNIBOX) {
        parent = TabAndroid::FromWebContents(params->source_contents);
      }

      // Create a WebContents.
      content::WebContents::CreateParams create_params(
          params->initiating_profile);
      std::unique_ptr<content::WebContents> web_contents =
          content::WebContents::Create(create_params);

      // Create a new tab.
      tabs::TabInterface* new_tab = tab_model->CreateTab(
          parent, std::move(web_contents), params->tabstrip_index, launch_type,
          /*should_pin=*/false);

      if (!new_tab || !new_tab->GetContents()) {
        return nullptr;
      }

      // Bring the new tab to the foreground if necessary.
      if (params->disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB) {
        tabs::TabHandle new_tab_handle = new_tab->GetHandle();
        tab_model->HighlightTabs(new_tab_handle, {new_tab_handle});
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
      raw_ptr<tabs::TabInterface> active_tab = tab_model->GetActiveTab();
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
