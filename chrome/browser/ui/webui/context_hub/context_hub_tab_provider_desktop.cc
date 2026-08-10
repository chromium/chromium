// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/context_hub/context_hub_tab_provider_desktop.h"

#include <vector>

#include "base/check.h"
#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"

namespace context_hub {

ContextHubTabProviderDesktop::ContextHubTabProviderDesktop(Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
}
ContextHubTabProviderDesktop::~ContextHubTabProviderDesktop() = default;

// Returns all open tabs across all browser windows for the profile.
std::vector<content::WebContents*> ContextHubTabProviderDesktop::GetTabs() {
  std::vector<content::WebContents*> tabs;
  ProfileBrowserCollection* collection =
      ProfileBrowserCollection::GetForProfile(profile_);
  if (!collection) {
    return tabs;
  }
  collection->ForEach([&](BrowserWindowInterface* browser) {
    TabStripModel* tab_strip_model = browser->GetTabStripModel();
    if (!tab_strip_model) {
      return true;
    }
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      if (content::WebContents* tab_contents =
              tab_strip_model->GetWebContentsAt(i)) {
        tabs.push_back(tab_contents);
      }
    }
    return true;
  });
  return tabs;
}

// Returns all open ungrouped tabs across all browser windows for the profile.
std::vector<content::WebContents*>
ContextHubTabProviderDesktop::GetUngroupedTabs() {
  std::vector<content::WebContents*> tabs;
  ProfileBrowserCollection* collection =
      ProfileBrowserCollection::GetForProfile(profile_);
  if (!collection) {
    return tabs;
  }
  collection->ForEach([&](BrowserWindowInterface* browser) {
    TabStripModel* tab_strip_model = browser->GetTabStripModel();
    if (!tab_strip_model || !tab_strip_model->SupportsTabGroups()) {
      return true;
    }
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      if (tab_strip_model->IsTabPinned(i) ||
          tab_strip_model->GetTabGroupForTab(i).has_value()) {
        continue;
      }
      if (content::WebContents* tab_contents =
              tab_strip_model->GetWebContentsAt(i)) {
        tabs.push_back(tab_contents);
      }
    }
    return true;
  });
  return tabs;
}

// Activates the tab matching the given session tab ID and brings its window to
// focus.
void ContextHubTabProviderDesktop::SwitchToTab(int64_t tab_id) {
  ProfileBrowserCollection* collection =
      ProfileBrowserCollection::GetForProfile(profile_);
  if (!collection) {
    return;
  }
  collection->ForEach([&](BrowserWindowInterface* browser) {
    TabStripModel* tab_strip_model = browser->GetTabStripModel();
    if (!tab_strip_model) {
      return true;
    }
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      content::WebContents* tab_contents =
          tab_strip_model->GetWebContentsAt(i);
      if (tab_contents) {
        SessionID session_id =
            sessions::SessionTabHelper::IdForTab(tab_contents);
        if (session_id.is_valid() && session_id.id() == tab_id) {
          tab_strip_model->ActivateTabAt(i);
          browser->GetWindow()->Show();
          return false;
        }
      }
    }
    return true;
  });
}

bool ContextHubTabProviderDesktop::ConfirmTabGroups(
    base::span<const TabGroupEntry> groups) {
  NOTIMPLEMENTED();
  return false;
}

void ContextHubTabProviderDesktop::RemoveGroupFromTabstripIfOpen(
    const base::Uuid& saved_guid) {
  NOTIMPLEMENTED();
}

void ContextHubTabProviderDesktop::UngroupGroupFromTabstripIfOpen(
    const base::Uuid& saved_guid) {
  NOTIMPLEMENTED();
}

}  // namespace context_hub
