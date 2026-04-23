// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator.h"

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
#include "chrome/common/webui_url_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"
#include "ui/base/window_open_disposition.h"

namespace {

// Returns true if NavigateParams are valid, false otherwise.
bool ValidNavigateParams(NavigateParams* params) {
  // TODO (crbug.com/441594986) Confirm this is correct.
  DCHECK(params->browser);
  DCHECK(!params->navigated_or_inserted_contents);
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

  TabListInterface* tab_list = TabListInterface::From(params->browser);
  bool empty_tab_list = tab_list && tab_list->GetTabCount() == 0;

  if (empty_tab_list &&
      (params->disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB ||
       params->disposition == WindowOpenDisposition::CURRENT_TAB)) {
    params->disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  }

  // Pre-fill navigated_or_inserted_contents. This is used as a flag
  // downstream to determine if contents_to_insert was provided.
  if (params->contents_to_insert) {
    params->navigated_or_inserted_contents = params->contents_to_insert.get();
  }

  return true;
}

bool IsNtpUrl(const GURL& url) {
  return url.host() == chrome::kChromeUINewTabHost &&
         (url.SchemeIs(content::kChromeUIScheme) ||
          url.SchemeIs(content::kChromeNativeScheme));
}

// Searches across all windows and tabs to locate a tab with the same url.
// If such a tab exists, params->browser is set to this tab, it is activated,
// and the original NTP is closed if applicable.
// If no tab exists, a fallback disposition is applied to params->disposition.
void TrySwitchToMatchingTab(NavigateParams* params) {
  std::pair<BrowserWindowInterface*, int> browser_and_index =
      GetIndexAndBrowserOfMatchingTab(params->initiating_profile, *params);

  if (!browser_and_index.first || browser_and_index.second < 0) {
    bool is_empty_source =
        params->source_contents &&
        (IsNtpUrl(params->source_contents->GetVisibleURL()) ||
         params->source_contents->GetVisibleURL() == url::kAboutBlankURL);

    params->disposition = is_empty_source
                              ? WindowOpenDisposition::CURRENT_TAB
                              : WindowOpenDisposition::NEW_FOREGROUND_TAB;
    return;
  }

  auto bwi = browser_and_index.first;
  auto* tab_list = TabListInterface::From(bwi);
  tabs::TabInterface* tab = tab_list->GetTab(browser_and_index.second);
  tabs::TabInterface* prev_active_tab =
      TabListInterface::From(params->browser)->GetActiveTab();

  // Activate window and tab being switched to.
  params->browser = bwi;
  params->browser->GetWindow()->Activate();
  tab_list->ActivateTab(tab->GetHandle());

  // Close the previously active tab if NTP, unless it has history.
  if (prev_active_tab) {
    content::NavigationController& controller =
        prev_active_tab->GetContents()->GetController();
    bool has_history = controller.CanGoBack() || controller.CanGoForward();
    if (tab != prev_active_tab && !has_history &&
        IsNtpUrl(prev_active_tab->GetContents()->GetVisibleURL())) {
      prev_active_tab->Close();
    }
  }
}

// Helper to create/locate windows.
// If params->contents_to_insert is non-null, std::move() will be called on it.
void GetOrCreateBrowserWindowForDisposition(
    NavigateParams* params,
    base::OnceCallback<void(BrowserWindowInterface*)> callback) {
  Profile* profile = params->initiating_profile;
  switch (params->disposition) {
    case WindowOpenDisposition::OFF_THE_RECORD:
      // The existing profile was already checked and is not OTR
      // so we get an OTR profile and create a new window.
      profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
      [[fallthrough]];
    case WindowOpenDisposition::NEW_WINDOW: {
      BrowserWindowCreateParams create_params(*profile, params->user_gesture);
      create_params.web_contents = std::move(params->contents_to_insert);
      CreateBrowserWindow(std::move(create_params), std::move(callback));
      break;
    }
    case WindowOpenDisposition::NEW_POPUP: {
      BrowserWindowCreateParams create_params(
          BrowserWindowInterface::Type::TYPE_POPUP, *profile,
          params->user_gesture);
      create_params.web_contents = std::move(params->contents_to_insert);
      CreateBrowserWindow(std::move(create_params), std::move(callback));
      break;
    }
    case WindowOpenDisposition::SWITCH_TO_TAB: {
      TrySwitchToMatchingTab(params);
    }
      [[fallthrough]];
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      [[fallthrough]];
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      [[fallthrough]];
    case WindowOpenDisposition::IGNORE_ACTION:
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

  bool is_background =
      params->disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB;
  bool is_link = ui::PageTransitionCoreTypeIs(params->transition,
                                              ui::PAGE_TRANSITION_LINK);
  // 1. Explicit Index:
  // If an explicit index is requested, use FROM_CHROME_UI. This type does NOT
  // trigger "adjacency" logic in Java, allowing the passed index to be
  // respected.
  if (params->tabstrip_index != -1) {
    return is_background ? TabLaunchType::FROM_SYNC_BACKGROUND
                         : TabLaunchType::FROM_CHROME_UI;
  }

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

// Helper to create a new tab.
// If params->contents_to_insert is non-null, std::move() will be called on it.
tabs::TabInterface* CreateNewTab(NavigateParams* params) {
  // On Android, TabListInterface is TabModel.
  TabModel* tab_model =
      static_cast<TabModel*>(TabListInterface::From(params->browser));
  if (!tab_model) {
    return nullptr;
  }

  // Select the TabLaunchType.
  TabModel::TabLaunchType launch_type = GetTabLaunchType(params);

  // Identify parent tab.
  // Parent tab is set to nullptr to avoid adjacency overrides when
  // launching from the Omnibox (where we always append to the end).
  TabAndroid* parent = nullptr;

  if (params->source_contents &&
      launch_type != TabModel::TabLaunchType::FROM_OMNIBOX) {
    parent = TabAndroid::FromWebContents(params->source_contents);
  }

  // Use the supplied WebContents or create a new one.
  std::unique_ptr<content::WebContents> web_contents;
  if (params->contents_to_insert) {
    web_contents = std::move(params->contents_to_insert);
  } else {
    content::WebContents::CreateParams create_params(
        params->initiating_profile);
    web_contents = content::WebContents::Create(create_params);
  }

  // Create a new tab.
  tabs::TabInterface* new_tab = tab_model->CreateTab(
      parent, std::move(web_contents), params->tabstrip_index, launch_type,
      /*should_pin=*/false);

  // Early out if Java tab creation failed.
  if (!new_tab || !new_tab->GetContents()) {
    return nullptr;
  }

  // Bring the new tab to the foreground if necessary.
  if (params->disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    tab_model->ActivateTab(new_tab->GetHandle());
  }

  // The new tab's WebContents is the target for our navigation.
  params->source_contents = new_tab->GetContents();
  return new_tab;
}

// Helper to create/locate tabs.
// If params->contents_to_insert is non-null, std::move() will be called on it.
tabs::TabInterface* GetOrCreateTabForDisposition(NavigateParams* params) {
  TabModel* tab_model =
      static_cast<TabModel*>(TabListInterface::From(params->browser));
  if (!tab_model) {
    return nullptr;
  }

  switch (params->disposition) {
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      [[fallthrough]];
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      return CreateNewTab(params);
    case WindowOpenDisposition::CURRENT_TAB:
      // CURRENT_TAB with contents_to_insert is handled the same as
      // NEW_FOREGROUND_TAB.
      if (params->contents_to_insert) {
        return CreateNewTab(params);
      }
      // Otherwise, return the source tab.
      if (params->source_contents) {
        return tabs::TabInterface::GetFromContents(params->source_contents);
      }
      // Otherwise use the active tab.
      [[fallthrough]];
    case WindowOpenDisposition::SWITCH_TO_TAB:
      [[fallthrough]];
    case WindowOpenDisposition::OFF_THE_RECORD:
      // A new incognito window has already been created with a new tab.
      [[fallthrough]];
    case WindowOpenDisposition::NEW_POPUP:
      [[fallthrough]];
    case WindowOpenDisposition::NEW_WINDOW: {
      // A new tab is already created when the new window is created on Android.
      // Just get the active tab.
      tabs::TabInterface* active_tab = tab_model->GetActiveTab();
      CHECK(active_tab);
      params->source_contents = active_tab->GetContents();
      return active_tab;
    }
    case WindowOpenDisposition::IGNORE_ACTION: {
      if (!params->source_contents) {
        tabs::TabInterface* active_tab = tab_model->GetActiveTab();
        if (active_tab) {
          params->source_contents = active_tab->GetContents();
        }
      }
      params->browser = nullptr;
      return nullptr;
    }
    default:
      NOTIMPLEMENTED();
      return nullptr;
  }
}

base::WeakPtr<content::NavigationHandle> GetTabAndPerformNavigation(
    NavigateParams* params) {
  bool is_contents_inserted = params->navigated_or_inserted_contents != nullptr;
  tabs::TabInterface* tab = GetOrCreateTabForDisposition(params);
  if (!tab || !tab->GetContents()) {
    // WindowOpenDisposition::IGNORE_ACTION exits here.
    return nullptr;
  }

  if (is_contents_inserted) {
    DCHECK_EQ(params->navigated_or_inserted_contents, tab->GetContents());
  }

  params->navigated_or_inserted_contents = tab->GetContents();

  // Skip navigation if we inserted existing contents.
  if (is_contents_inserted || !params->source_contents) {
    return nullptr;
  }

  // If SWITCH_TO_TAB found the tab, skip navigation unless crashed or
  // IGNORE_AND_NAVIGATE with different URL.
  if (params->disposition == WindowOpenDisposition::SWITCH_TO_TAB) {
    content::WebContents* contents = params->navigated_or_inserted_contents;

    if (contents->IsCrashed()) {
      contents->GetController().Reload(content::ReloadType::NORMAL, true);
      return nullptr;
    }

    bool should_navigate_anyway =
        params->path_behavior == NavigateParams::IGNORE_AND_NAVIGATE &&
        contents->GetURL() != params->url;

    if (!should_navigate_anyway) {
      return nullptr;
    }
  }

  // Perform navigation.
  content::NavigationController::LoadURLParams load_url_params =
      LoadURLParamsFromNavigateParams(params->navigated_or_inserted_contents,
                                      params);
  return params->navigated_or_inserted_contents->GetController()
      .LoadURLWithParams(load_url_params);
}

void GetTabAndPerformNavigationAsync(
    NavigateParams* params,
    base::OnceCallback<void(base::WeakPtr<content::NavigationHandle>)> callback,
    BrowserWindowInterface* bwi) {
  base::WeakPtr<content::NavigationHandle> handle = nullptr;
  if (bwi) {
    params->browser = bwi;
    handle = GetTabAndPerformNavigation(params);
  }

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
      params->disposition != WindowOpenDisposition::IGNORE_ACTION &&
      params->disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB &&
      params->disposition != WindowOpenDisposition::NEW_FOREGROUND_TAB &&
      params->disposition != WindowOpenDisposition::SWITCH_TO_TAB) {
    return nullptr;
  }

  // This call may activate a different window, but no new window will be
  // created.
  if (params->disposition == WindowOpenDisposition::SWITCH_TO_TAB) {
    TrySwitchToMatchingTab(params);
  }

  return GetTabAndPerformNavigation(params);
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
      params, base::BindOnce(&GetTabAndPerformNavigationAsync, params,
                             std::move(callback)));
}
