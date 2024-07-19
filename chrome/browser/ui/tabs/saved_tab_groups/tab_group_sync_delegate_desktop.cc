// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_delegate_desktop.h"

#include "base/containers/contains.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_service_wrapper.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"

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

}  // namespace

TabGroupSyncDelegateDesktop::TabGroupSyncDelegateDesktop(
    TabGroupSyncService* service,
    Profile* profile)
    : wrapper_service_(std::make_unique<TabGroupServiceWrapper>(
          service,
          /*saved_tab_group_keyed_service=*/nullptr)),
      listener_(
          std::make_unique<SavedTabGroupModelListener>(wrapper_service_.get(),
                                                       profile)) {}

TabGroupSyncDelegateDesktop::~TabGroupSyncDelegateDesktop() = default;

void TabGroupSyncDelegateDesktop::HandleOpenTabGroupRequest(
    const base::Uuid& sync_tab_group_id,
    std::unique_ptr<TabGroupActionContext> context) {
  const std::optional<SavedTabGroup> group =
      wrapper_service_->GetGroup(sync_tab_group_id);

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
  std::map<content::WebContents*, base::Uuid> opened_web_contents_to_uuid =
      OpenTabsAndMapWebcontentsToTabUUIDs(browser, group.value());

  if (opened_web_contents_to_uuid.empty()) {
    // If not tabs were opened, do nothing.
    return;
  }

  // Add the tabs to a new group in the tabstrip and link it to `group`.
  AddOpenedTabsToGroup(browser->tab_strip_model(),
                       std::move(opened_web_contents_to_uuid), group.value());
}

void TabGroupSyncDelegateDesktop::CreateLocalTabGroup(
    const SavedTabGroup& tab_group) {
  // TODO(b/346871861): Implement.
}

void TabGroupSyncDelegateDesktop::CloseLocalTabGroup(
    const LocalTabGroupID& local_id) {
  // TODO(b/346871861): Implement.
}

void TabGroupSyncDelegateDesktop::UpdateLocalTabGroup(
    const SavedTabGroup& group) {
  // TODO(b/346871861): Implement.
}

std::vector<LocalTabGroupID>
TabGroupSyncDelegateDesktop::GetLocalTabGroupIds() {
  // TODO(b/346871861): Implement.
  return std::vector<LocalTabGroupID>();
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

std::map<content::WebContents*, base::Uuid>
TabGroupSyncDelegateDesktop::OpenTabsAndMapWebcontentsToTabUUIDs(
    Browser* const browser,
    const SavedTabGroup& saved_group) {
  std::map<content::WebContents*, base::Uuid> web_contents;
  for (const SavedTabGroupTab& saved_tab : saved_group.saved_tabs()) {
    if (!saved_tab.url().is_valid()) {
      continue;
    }

    auto* navigation_handle = SavedTabGroupUtils::OpenTabInBrowser(
        saved_tab.url(), browser, browser->profile(),
        WindowOpenDisposition::NEW_BACKGROUND_TAB);
    content::WebContents* created_contents =
        navigation_handle ? navigation_handle->GetWebContents() : nullptr;

    if (!created_contents) {
      continue;
    }

    web_contents.emplace(created_contents, saved_tab.saved_tab_guid());
  }

  return web_contents;
}

TabGroupId TabGroupSyncDelegateDesktop::AddOpenedTabsToGroup(
    TabStripModel* tab_strip_model,
    const std::map<content::WebContents*, base::Uuid>&
        opened_web_contents_to_uuid,
    const SavedTabGroup& saved_group) {
  std::vector<int> tab_indices;
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    if (base::Contains(opened_web_contents_to_uuid,
                       tab_strip_model->GetWebContentsAt(i)) &&
        !tab_strip_model->GetTabGroupForTab(i).has_value()) {
      tab_indices.push_back(i);
    }
  }

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  tab_strip_model->AddToGroupForRestore(tab_indices, tab_group_id);

  wrapper_service_->UpdateLocalTabGroupMapping(saved_group.saved_guid(),
                                               tab_group_id);

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
      wrapper_service_->GetGroup(saved_group.saved_guid());

  listener_->ConnectToLocalTabGroup(*saved_group2, opened_web_contents_to_uuid);
  return tab_group_id;
}
}  // namespace tab_groups
