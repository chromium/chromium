// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_delegate_desktop.h"

#include <iterator>
#include <map>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/not_fatal_until.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_proxy.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "ui/gfx/range/range.h"

namespace tab_groups {
namespace {

class ScopedLocalObservationPauserImpl : public ScopedLocalObservationPauser {
 public:
  explicit ScopedLocalObservationPauserImpl(
      SavedTabGroupModelListener* listener);
  ~ScopedLocalObservationPauserImpl() override;

  // Disallow copy/assign.
  ScopedLocalObservationPauserImpl(const ScopedLocalObservationPauserImpl&) =
      delete;
  ScopedLocalObservationPauserImpl& operator=(
      const ScopedLocalObservationPauserImpl&) = delete;

 private:
  raw_ptr<SavedTabGroupModelListener> listener_;
};

ScopedLocalObservationPauserImpl::ScopedLocalObservationPauserImpl(
    SavedTabGroupModelListener* listener)
    : listener_(listener) {
  listener_->PauseLocalObservation();
}

ScopedLocalObservationPauserImpl::~ScopedLocalObservationPauserImpl() {
  listener_->ResumeLocalObservation();
}

// Removes the tab from the group and closes it.
void RemoveTabFromGroup(TabStripModel& tab_strip_model,
                        const tabs::TabInterface& local_tab) {
  // Unload listeners can delay or prevent a tab closing. Remove the tab from
  // the group first so the local and saved groups can be consistent even if
  // this happens. Do not store the index of the tab before removing it from the
  // group because removing it from the group may have moved the tab to maintain
  // group contiguity.
  tab_strip_model.RemoveFromGroup({tab_strip_model.GetIndexOfTab(&local_tab)});

  // Find the tab again and close it.
  tab_strip_model.CloseWebContentsAt(
      tab_strip_model.GetIndexOfTab(&local_tab),
      TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}

TabStripModel* GetTabStripModelForLocalGroup(const LocalTabGroupID& group_id) {
  Browser* browser = SavedTabGroupUtils::GetBrowserWithTabGroupId(group_id);
  CHECK(browser);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  CHECK(tab_strip_model);
  CHECK(tab_strip_model->SupportsTabGroups());

  return tab_strip_model;
}

// Removes the last tabs from the local group so that it has the same number of
// tabs as the saved group. Does nothing if the local group has less or equal
// number of tabs than the saved group.
void RemoveExtraTabsFromLocalGroupBeforeConnecting(
    const SavedTabGroup& saved_group) {
  CHECK(saved_group.local_group_id().has_value());

  const LocalTabGroupID& group_id = saved_group.local_group_id().value();
  TabStripModel* tab_strip_model = GetTabStripModelForLocalGroup(group_id);
  TabGroup* tab_group = tab_strip_model->group_model()->GetTabGroup(group_id);
  CHECK(tab_group);

  // Collect the tabs to close because the range and tab indexes may change
  // during removing the tabs from the group.
  gfx::Range tab_range = tab_group->ListTabs();
  std::vector<const tabs::TabInterface*> tabs_to_close;
  for (size_t i = saved_group.saved_tabs().size(); i < tab_range.length();
       ++i) {
    tabs_to_close.push_back(
        tab_strip_model->GetTabAtIndex(i + tab_range.start()));
  }
  for (const tabs::TabInterface* const tab : tabs_to_close) {
    CHECK(tab);
    RemoveTabFromGroup(*tab_strip_model, *tab);
  }
}

// Try to open the `saved_tab`. Returns the opened tab if successful, otherwise
// returns nullptr.
tabs::TabInterface* MaybeOpenTabFromSavedTab(const SavedTabGroupTab& saved_tab,
                                             Browser* browser) {
  if (!saved_tab.url().is_valid()) {
    return nullptr;
  }

  content::NavigationHandle* navigation_handle =
      SavedTabGroupUtils::OpenTabInBrowser(
          saved_tab.url(), browser, browser->profile(),
          WindowOpenDisposition::NEW_BACKGROUND_TAB);
  if (!navigation_handle) {
    return nullptr;
  }

  return browser->tab_strip_model()->GetTabForWebContents(
      navigation_handle->GetWebContents());
}

}  // namespace

TabGroupSyncDelegateDesktop::TabGroupSyncDelegateDesktop(
    TabGroupSyncService* service,
    Profile* profile)
    : service_(service),
      listener_(
          std::make_unique<SavedTabGroupModelListener>(service_, profile)) {}

TabGroupSyncDelegateDesktop::~TabGroupSyncDelegateDesktop() = default;

void TabGroupSyncDelegateDesktop::HandleOpenTabGroupRequest(
    const base::Uuid& sync_tab_group_id,
    std::unique_ptr<TabGroupActionContext> context) {
  const std::optional<SavedTabGroup> group =
      service_->GetGroup(sync_tab_group_id);

  // In the case where this function is called after confirmation of an
  // interstitial, the saved_group could be null, so protect against this by
  // early returning.
  if (!group.has_value()) {
    return;
  }

  // Activate the first tab in a group if it is already open.
  if (group->local_group_id().has_value()) {
    SavedTabGroupUtils::FocusFirstTabOrWindowInOpenGroup(
        group->local_group_id().value());
    return;
  }

  TabGroupActionContextDesktop* desktop_context =
      static_cast<TabGroupActionContextDesktop*>(context.get());
  Browser* const browser = desktop_context->browser;

  // Open the tabs in the saved group.
  std::map<tabs::TabInterface*, base::Uuid> tab_guid_mapping =
      OpenTabsAndMapToUuids(browser, group.value());

  if (tab_guid_mapping.empty()) {
    // If not tabs were opened, do nothing.
    return;
  }

  // Add the tabs to a new group in the tabstrip and link it to `group`.
  AddOpenedTabsToGroup(browser->tab_strip_model(), std::move(tab_guid_mapping),
                       group.value());
}

void TabGroupSyncDelegateDesktop::CreateLocalTabGroup(
    const SavedTabGroup& tab_group) {
  // On desktop we do not automatically open new saved tab groups. Instead, new
  // groups appear as chips in the bookmarks bar if pinned, and as entries in
  // the everything menu.
}

void TabGroupSyncDelegateDesktop::CloseLocalTabGroup(
    const LocalTabGroupID& local_id) {
  CHECK(!service_->GetGroup(local_id));
  listener_->RemoveLocalGroupFromSync(local_id);
}

void TabGroupSyncDelegateDesktop::ConnectLocalTabGroup(
    const SavedTabGroup& group) {
  if (!group.local_group_id().has_value()) {
    // There is no a corresponding local group to connect.
    return;
  }

  const LocalTabGroupID& group_id = group.local_group_id().value();
  if (listener_->IsTrackingLocalTabGroup(group_id)) {
    // The local group is already being tracked.
    return;
  }

  // Before tracking, the local group needs to close tabs if there are more tabs
  // than in the saved group. This is needed because ConnectToLocalTabGroup()
  // expects that there exists a corresponding saved tab for each local tab.
  RemoveExtraTabsFromLocalGroupBeforeConnecting(group);

  TabStripModel* tab_strip_model = GetTabStripModelForLocalGroup(group_id);
  TabGroup* tab_group = tab_strip_model->group_model()->GetTabGroup(group_id);
  CHECK(tab_group);

  // Associate all the local tabs with the saved group, note that it's done on
  // best effort without verifying if they match or valid.
  gfx::Range tab_range = tab_group->ListTabs();
  CHECK_LE(tab_range.length(), group.saved_tabs().size());
  std::map<tabs::TabInterface*, base::Uuid> tab_guid_mapping;
  for (size_t i = 0; i < tab_range.length(); ++i) {
    tabs::TabInterface* const tab =
        tab_strip_model->GetTabAtIndex(tab_range.start() + i);
    CHECK(tab);
    tab_guid_mapping.emplace(tab, group.saved_tabs()[i].saved_tab_guid());
  }

  listener_->ConnectToLocalTabGroup(group, std::move(tab_guid_mapping));
  CHECK(listener_->IsTrackingLocalTabGroup(group_id));

  // Update the local tab group based on the new connected saved group.
  UpdateLocalTabGroup(group);
}

void TabGroupSyncDelegateDesktop::DisconnectLocalTabGroup(
    const LocalTabGroupID& local_id) {
  listener_->DisconnectLocalTabGroup(local_id, ClosingSource::kDeletedByUser);
}

void TabGroupSyncDelegateDesktop::UpdateLocalTabGroup(
    const SavedTabGroup& group) {
  if (!group.local_group_id().has_value()) {
    return;
  }

  const LocalTabGroupID& group_id = group.local_group_id().value();
  CHECK(listener_->IsTrackingLocalTabGroup(group_id),
        base::NotFatalUntil::M135);

  // Update the local group with the new data. This will open new tabs, close
  // tabs, and navigate tabs to match the saved group.
  listener_->UpdateLocalGroupFromSync(group_id);
}

std::vector<LocalTabGroupID>
TabGroupSyncDelegateDesktop::GetLocalTabGroupIds() {
  std::vector<LocalTabGroupID> local_group_ids;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->tab_strip_model() &&
        browser->tab_strip_model()->SupportsTabGroups()) {
      std::vector<LocalTabGroupID> local_groups =
          browser->tab_strip_model()->group_model()->ListTabGroups();
      base::ranges::copy(local_groups, std::back_inserter(local_group_ids));
    }
  }

  return local_group_ids;
}

std::vector<LocalTabID> TabGroupSyncDelegateDesktop::GetLocalTabIdsForTabGroup(
    const LocalTabGroupID& local_tab_group_id) {
  // TODO(b/346871861): Implement.
  return std::vector<LocalTabID>();
}

void TabGroupSyncDelegateDesktop::CreateRemoteTabGroup(
    const LocalTabGroupID& local_tab_group_id) {
  // TODO(b/346871861): Implement.
}

std::unique_ptr<ScopedLocalObservationPauser>
TabGroupSyncDelegateDesktop::CreateScopedLocalObserverPauser() {
  return std::make_unique<ScopedLocalObservationPauserImpl>(listener_.get());
}

std::map<tabs::TabInterface*, base::Uuid>
TabGroupSyncDelegateDesktop::OpenTabsAndMapToUuids(
    Browser* const browser,
    const SavedTabGroup& saved_group) {
  std::map<tabs::TabInterface*, base::Uuid> tab_guid_mapping;
  for (const SavedTabGroupTab& saved_tab : saved_group.saved_tabs()) {
    tabs::TabInterface* const tab =
        MaybeOpenTabFromSavedTab(saved_tab, browser);
    if (tab) {
      tab_guid_mapping.emplace(tab, saved_tab.saved_tab_guid());
    }
  }

  return tab_guid_mapping;
}

TabGroupId TabGroupSyncDelegateDesktop::AddOpenedTabsToGroup(
    TabStripModel* tab_strip_model,
    const std::map<tabs::TabInterface*, base::Uuid>& tab_guid_mapping,
    const SavedTabGroup& saved_group) {
  std::vector<int> tab_indices;
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    if (base::Contains(tab_guid_mapping, tab_strip_model->GetTabAtIndex(i)) &&
        !tab_strip_model->GetTabGroupForTab(i).has_value()) {
      tab_indices.push_back(i);
    }
  }

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  tab_strip_model->AddToGroupForRestore(tab_indices, tab_group_id);

  service_->UpdateLocalTabGroupMapping(saved_group.saved_guid(), tab_group_id,
                                       OpeningSource::kOpenedFromRevisitUi);

  TabGroup* const tab_group =
      tab_strip_model->group_model()->GetTabGroup(tab_group_id);

  // Activate the first tab in the group.
  std::optional<int> first_tab = tab_group->GetFirstTab();
  DCHECK(first_tab.has_value());
  tab_strip_model->ActivateTabAt(first_tab.value());

  // Update the group to use the saved title and color.
  TabGroupVisualData visual_data(saved_group.title(), saved_group.color(),
                                 /*is_collapsed=*/false);
  tab_group->SetVisualData(visual_data, /*is_customized=*/true);

  const std::optional<SavedTabGroup> saved_group2 =
      service_->GetGroup(saved_group.saved_guid());

  listener_->ConnectToLocalTabGroup(*saved_group2, tab_guid_mapping);
  return tab_group_id;
}
}  // namespace tab_groups
