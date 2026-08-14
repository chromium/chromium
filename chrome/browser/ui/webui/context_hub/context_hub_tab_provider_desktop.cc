// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/context_hub/context_hub_tab_provider_desktop.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/context_hub/context_hub_service.h"
#include "chrome/browser/context_hub/context_hub_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"

namespace context_hub {
namespace {

using WindowTabIndicesMap =
    base::flat_map<BrowserWindowInterface*, std::vector<int>>;

// Finds ungrouped tab indices in a TabStripModel matching a set of session tab
// IDs.
std::vector<int> GetMatchingUngroupedIndices(
    TabStripModel* model,
    const base::flat_set<int64_t>& tab_ids) {
  std::vector<int> indices;
  if (!model || !model->SupportsTabGroups()) {
    return indices;
  }
  for (int i = 0; i < model->count(); ++i) {
    if (model->IsTabPinned(i) || model->GetTabGroupForTab(i).has_value()) {
      continue;
    }
    if (content::WebContents* wc = model->GetWebContentsAt(i)) {
      SessionID session_id = sessions::SessionTabHelper::IdForTab(wc);
      if (session_id.is_valid() && tab_ids.contains(session_id.id())) {
        indices.push_back(i);
      }
    }
  }
  return indices;
}

// Maps browser windows to their ungrouped tab indices matching the group's tab
// IDs.
WindowTabIndicesMap FindUngroupedTabsForGroup(
    Profile* profile,
    const base::flat_set<int64_t>& group_tab_ids) {
  WindowTabIndicesMap window_indices;
  ProfileBrowserCollection* collection =
      ProfileBrowserCollection::GetForProfile(profile);
  if (!collection) {
    return window_indices;
  }
  collection->ForEach(
      [&group_tab_ids, &window_indices](BrowserWindowInterface* b) {
        std::vector<int> indices =
            GetMatchingUngroupedIndices(b->GetTabStripModel(), group_tab_ids);
        if (!indices.empty()) {
          window_indices[b] = std::move(indices);
        }
        return true;
      });
  return window_indices;
}

// Returns the browser window containing the most tabs for the target group.
// TODO(crbug.com/542259689): Add tiebreaker using MRU window.
BrowserWindowInterface* GetMajorityBrowser(
    const WindowTabIndicesMap& window_indices) {
  auto it = std::ranges::max_element(window_indices, {}, [](const auto& entry) {
    return entry.second.size();
  });
  return it != window_indices.end() ? it->first : nullptr;
}

// Moves ungrouped tabs from other browser windows into the target browser
// window.
void MoveTabsToBrowser(BrowserWindowInterface* target_browser,
                       const WindowTabIndicesMap& window_indices) {
  for (const auto& [source_browser, indices] : window_indices) {
    if (source_browser != target_browser) {
      chrome::MoveTabsToExistingWindow(source_browser, target_browser, indices);
    }
  }
}

// Creates a new tab group in the browser window and applies the group visual
// metadata.
bool GroupTabsInWindow(BrowserWindowInterface* browser,
                       const base::flat_set<int64_t>& group_tab_ids,
                       std::string_view label) {
  TabStripModel* tab_strip = browser->GetTabStripModel();
  // Tabs moved from other windows have new positions
  // Rescan the target window to find all matching indices.
  std::vector<int> final_indices =
      GetMatchingUngroupedIndices(tab_strip, group_tab_ids);
  if (final_indices.empty()) {
    return false;
  }

  tab_groups::TabGroupId group_id = tab_strip->AddToNewGroup(final_indices);
  tab_groups::TabGroupVisualData visual_data(base::UTF8ToUTF16(label),
                                             tab_groups::TabGroupColorId::kBlue,
                                             /*is_collapsed=*/false);
  tab_strip->ChangeTabGroupVisuals(group_id, visual_data);
  return true;
}

// Consolidates and groups tabs across windows for a single tab group entry.
bool ConfirmSingleTabGroup(Profile* profile, const TabGroupEntry& group) {
  if (group.tab_ids.empty()) {
    return false;
  }

  base::flat_set<int64_t> tab_ids(group.tab_ids);
  WindowTabIndicesMap window_indices =
      FindUngroupedTabsForGroup(profile, tab_ids);
  if (window_indices.empty()) {
    return false;
  }

  BrowserWindowInterface* target_browser = GetMajorityBrowser(window_indices);
  if (!target_browser) {
    return false;
  }

  MoveTabsToBrowser(target_browser, window_indices);
  return GroupTabsInWindow(target_browser, tab_ids, group.label);
}

}  // namespace

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
      content::WebContents* tab_contents = tab_strip_model->GetWebContentsAt(i);
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

// Closes the tab matching the given session tab ID.
void ContextHubTabProviderDesktop::CloseTab(int64_t tab_id) {
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
      content::WebContents* tab_contents = tab_strip_model->GetWebContentsAt(i);
      if (tab_contents) {
        SessionID session_id =
            sessions::SessionTabHelper::IdForTab(tab_contents);
        if (session_id.is_valid() && session_id.id() == tab_id) {
          tab_strip_model->CloseWebContentsAt(
              i, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB |
                     TabCloseTypes::CLOSE_USER_GESTURE);
          return false;
        }
      }
    }
    return true;
  });
}

// Groups tabs in the tab strip for all provided tab group entries.
// TODO(crbug.com/546250053): Add support for confirming select subset of
// groups.
bool ContextHubTabProviderDesktop::ConfirmTabGroups(
    base::span<const TabGroupEntry> groups) {
  bool created_any = false;
  for (const auto& group : groups) {
    if (ConfirmSingleTabGroup(profile_, group)) {
      created_any = true;
    }
  }
  return created_any;
}

// Closes the specified tab group from the browser window if currently open.
void ContextHubTabProviderDesktop::RemoveGroupFromTabstripIfOpen(
    const base::Uuid& saved_guid) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    return;
  }
  std::optional<tab_groups::LocalTabGroupID> local_id =
      service->GetLocalGroupIdForConfirmedGroup(saved_guid);
  if (!local_id.has_value()) {
    return;
  }

  Browser* browser = tab_groups::SavedTabGroupUtils::GetBrowserWithTabGroupId(
      local_id.value());
  if (browser) {
    tab_groups::SavedTabGroupUtils::RemoveGroupFromTabstrip(browser,
                                                            local_id.value());
  }
}

// Dissolves the tab group in the tab strip while keeping all member tabs open.
void ContextHubTabProviderDesktop::UngroupGroupFromTabstripIfOpen(
    const base::Uuid& saved_guid) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    return;
  }
  std::optional<tab_groups::LocalTabGroupID> local_id =
      service->GetLocalGroupIdForConfirmedGroup(saved_guid);
  if (!local_id.has_value()) {
    return;
  }

  ProfileBrowserCollection* collection =
      ProfileBrowserCollection::GetForProfile(profile_);
  if (!collection) {
    return;
  }

  collection->ForEach([&](BrowserWindowInterface* browser) {
    TabStripModel* tab_strip = browser->GetTabStripModel();
    if (!tab_strip || !tab_strip->SupportsTabGroups()) {
      return true;
    }
    std::vector<int> tabs;
    for (int i = 0; i < tab_strip->count(); ++i) {
      if (tab_strip->GetTabGroupForTab(i) == local_id.value()) {
        tabs.push_back(i);
      }
    }
    if (!tabs.empty()) {
      tab_strip->RemoveFromGroup(tabs);
      return false;
    }
    return true;
  });
}

}  // namespace context_hub
