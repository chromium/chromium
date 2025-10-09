// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator.h"

#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_navigator_params_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

base::WeakPtr<content::NavigationHandle> Navigate(NavigateParams* params) {
  // PRE-CHECKS
  // TODO (crbug.com/441594986) Confirm this is correct.
  DCHECK(params->browser);
  DCHECK(!params->contents_to_insert);
  DCHECK(!params->switch_to_singleton_tab);

  BrowserWindowInterface* source_browser = params->browser;
  params->initiating_profile = source_browser->GetProfile();
  if (params->initiating_profile->ShutdownStarted()) {
    // Don't navigate when the profile is shutting down.
    return nullptr;
  }
  DCHECK(params->initiating_profile);

  TabListInterface* tab_list = TabListInterface::From(params->browser);

  // HANDLE DISPOSITIONS
  // TODO (crbug.com/441594986) Clean this by breaking it into functions.
  switch (params->disposition) {
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      [[fallthrough]];
    case WindowOpenDisposition::NEW_FOREGROUND_TAB: {
      if (!tab_list) {
        return nullptr;
      }
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
      tabs::TabInterface* new_tab =
          tab_list->OpenTab(params->url, insertion_index);
      if (!new_tab || !new_tab->GetContents()) {
        return nullptr;
      }

      // Bring the new tab to the foreground if necessary.
      if (params->disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) {
        tabs::TabHandle new_tab_handle = new_tab->GetHandle();
        tab_list->HighlightTabs(new_tab_handle, {new_tab_handle});
      }

      // The new tab's WebContents is the target for our navigation.
      params->source_contents = new_tab->GetContents();
      break;
    }
    case WindowOpenDisposition::CURRENT_TAB: {
      // If no source WebContents was specified, use the active one.
      if (!params->source_contents) {
        if (tab_list && tab_list->GetActiveTab()) {
          params->source_contents = tab_list->GetActiveTab()->GetContents();
        }
      }
      break;
    }
    default:
      NOTIMPLEMENTED();
      return nullptr;
  }

  if (!params->source_contents) {
    return nullptr;
  }

  // Perform the actual navigation on the determined source_contents.
  content::NavigationController::LoadURLParams load_url_params =
      LoadURLParamsFromNavigateParams(params);
  return params->source_contents->GetController().LoadURLWithParams(
      load_url_params);
}
