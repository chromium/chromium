// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/tab_groups_organizer_page_handler.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_menu_utils.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_tabs_menu_model.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

namespace {

struct SortableTabGroup {
  organizer_panel::mojom::TabGroupPtr group;
  base::Time last_used_time;
};

base::Time GetLastUsedTime(const tab_groups::SavedTabGroup& group) {
  if (!group.last_user_interaction_time().is_null()) {
    return group.last_user_interaction_time();
  }
  if (!group.update_time().is_null()) {
    return group.update_time();
  }
  return group.creation_time();
}

base::Time GetLastUsedTime(TabStripModel* tab_strip_model,
                           const TabGroup* tab_group) {
  base::Time last_used_time;
  const gfx::Range tab_range = tab_group->ListTabs();
  for (tabs::TabInterface* tab :
       tab_strip_model->GetTabsAtIndices(tab_range.ToIntVector())) {
    if (tab && tab->GetContents()) {
      last_used_time =
          std::max(last_used_time, tab->GetContents()->GetLastActiveTime());
    }
  }
  return last_used_time;
}

organizer_panel::mojom::TabGroupPtr CreateMojoTabGroup(
    const tab_groups::SavedTabGroup& group) {
  auto tab_group = organizer_panel::mojom::TabGroup::New();
  tab_group->id = group.saved_guid().AsLowercaseString();
  tab_group->title = base::UTF16ToUTF8(
      tab_groups::TabGroupMenuUtils::GetMenuTextForGroup(group));
  tab_group->color = group.color();
  tab_group->is_open = group.local_group_id().has_value();
  return tab_group;
}

organizer_panel::mojom::TabGroupPtr CreateMojoTabGroup(const TabGroup* group) {
  auto tab_group = CreateMojoTabGroup(
      tab_groups::SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
          group->id()));
  tab_group->id = tab_groups::LocalTabGroupIDToString(group->id());
  return tab_group;
}

}  // namespace

TabGroupsOrganizerPageHandler::TabGroupsOrganizerPageHandler(
    mojo::PendingReceiver<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
        receiver,
    mojo::PendingRemote<organizer_panel::mojom::TabGroupsOrganizerPage> page,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_contents_(web_contents),
      tab_group_sync_service_(
          tab_groups::TabGroupSyncServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
  if (tab_group_sync_service_) {
    tab_group_sync_service_observation_.Observe(tab_group_sync_service_);
  }
  if (TabStripModel* tab_strip_model = GetTabStripModel()) {
    tab_strip_model->AddObserver(this);
  }
}

TabGroupsOrganizerPageHandler::~TabGroupsOrganizerPageHandler() {
  if (TabStripModel* tab_strip_model = GetTabStripModel()) {
    tab_strip_model->RemoveObserver(this);
  }
  if (on_menu_closed_callback_) {
    std::move(on_menu_closed_callback_).Run();
  }
}

void TabGroupsOrganizerPageHandler::GetTabGroups(
    GetTabGroupsCallback callback) {
  std::vector<SortableTabGroup> sortable_groups;

  if (tab_group_sync_service_) {
    std::vector<tab_groups::SavedTabGroup> groups =
        tab_group_sync_service_->GetAllGroups();
    for (const tab_groups::SavedTabGroup& group : groups) {
      if (group.saved_tabs().empty()) {
        continue;
      }
      sortable_groups.push_back(
          {CreateMojoTabGroup(group), GetLastUsedTime(group)});
    }
  }

  // TODO(crbug.com/565021302): Listen to other tab strips for unsaved tab
  // groups.
  if (TabStripModel* tab_strip_model = GetTabStripModel();
      tab_strip_model && tab_strip_model->SupportsTabGroups()) {
    for (const tab_groups::TabGroupId& group_id :
         tab_strip_model->group_model()->ListTabGroups()) {
      if (const TabGroup* tab_group = GetUnsavedTabGroup(group_id)) {
        sortable_groups.push_back(
            {CreateMojoTabGroup(tab_group),
             GetLastUsedTime(tab_strip_model, tab_group)});
      }
    }
  }

  std::ranges::sort(sortable_groups,
                    [](const SortableTabGroup& a, const SortableTabGroup& b) {
                      return a.last_used_time > b.last_used_time;
                    });

  std::vector<organizer_panel::mojom::TabGroupPtr> tab_groups;
  tab_groups.reserve(sortable_groups.size());
  for (SortableTabGroup& entry : sortable_groups) {
    tab_groups.push_back(std::move(entry.group));
  }

  std::move(callback).Run(std::move(tab_groups));
}

void TabGroupsOrganizerPageHandler::OpenTabGroup(const std::string& id) {
  const base::Uuid uuid = base::Uuid::ParseLowercase(id);
  if (tab_group_sync_service_ && uuid.is_valid()) {
    const std::optional<tab_groups::SavedTabGroup> group =
        tab_group_sync_service_->GetGroup(uuid);
    if (group && !group->saved_tabs().empty()) {
      BrowserWindowInterface* browser =
          webui::GetBrowserWindowInterface(web_contents_);
      CHECK(browser);

      tab_groups::SavedTabGroupUtils::OpenSavedTabGroup(
          browser, group->saved_guid(),
          tab_groups::OpeningSource::kOpenedFromRevisitUi,
          tab_group_sync_service_);
      return;
    }
  }

  std::optional<tab_groups::LocalTabGroupID> group_id =
      tab_groups::LocalTabGroupIDFromString(id);
  if (group_id && GetUnsavedTabGroup(*group_id)) {
    tab_groups::SavedTabGroupUtils::FocusFirstTabOrWindowInOpenGroup(*group_id);
  }
}

void TabGroupsOrganizerPageHandler::ShowContextMenu(
    const std::string& group_id,
    const gfx::Rect& anchor_rect,
    ShowContextMenuCallback callback) {
  // If the menu was already open, close it.
  OnContextMenuClosed();

  on_menu_closed_callback_ = std::move(callback);

  std::optional<tab_groups::SavedTabGroup> saved_group;
  const base::Uuid uuid = base::Uuid::ParseLowercase(group_id);
  if (tab_group_sync_service_ && uuid.is_valid()) {
    saved_group = tab_group_sync_service_->GetGroup(uuid);
  }
  if (!saved_group.has_value()) {
    std::optional<tab_groups::LocalTabGroupID> local_group_id =
        tab_groups::LocalTabGroupIDFromString(group_id);
    if (local_group_id && GetUnsavedTabGroup(*local_group_id)) {
      saved_group =
          tab_groups::SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
              *local_group_id);
    }
  }
  if (!saved_group.has_value()) {
    OnContextMenuClosed();
    return;
  }

  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_contents_);
  if (!browser) {
    OnContextMenuClosed();
    return;
  }

  views::Widget* widget = views::Widget::GetTopLevelWidgetForNativeView(
      web_contents_->GetNativeView());
  if (!widget) {
    OnContextMenuClosed();
    return;
  }

  const gfx::Rect container_bounds = web_contents_->GetContainerBounds();
  if (!gfx::Rect(container_bounds.size()).Contains(anchor_rect)) {
    OnContextMenuClosed();
    return;
  }

  latest_command_id_ = 0;
  menu_model_ = std::make_unique<tab_groups::STGTabsMenuModel>(
      browser, tab_groups::TabGroupMenuContext::ORGANIZER_PANEL);
  menu_model_->Build(
      saved_group.value(),
      base::BindRepeating(
          &TabGroupsOrganizerPageHandler::GetAndIncrementLatestCommandId,
          base::Unretained(this)));

  gfx::Rect screen_rect = anchor_rect + container_bounds.OffsetFromOrigin();

  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(),
      views::MenuRunner::CONTEXT_MENU | views::MenuRunner::IS_NESTED,
      base::BindRepeating(&TabGroupsOrganizerPageHandler::OnContextMenuClosed,
                          weak_ptr_factory_.GetWeakPtr()));
  context_menu_runner_->RunMenuAt(widget, nullptr, screen_rect,
                                  views::MenuAnchorPosition::kTopLeft,
                                  ui::mojom::MenuSourceType::kNone);
}

bool TabGroupsOrganizerPageHandler::IsContextMenuRunningForTesting() const {
  return context_menu_runner_ && context_menu_runner_->IsRunning();
}

void TabGroupsOrganizerPageHandler::OnTabGroupAdded(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  if (group.saved_tabs().empty()) {
    return;
  }
  if (group.local_group_id().has_value()) {
    page_->TabGroupRemoved(
        tab_groups::LocalTabGroupIDToString(*group.local_group_id()));
  }
  page_->TabGroupAdded(CreateMojoTabGroup(group));
}

void TabGroupsOrganizerPageHandler::OnTabGroupUpdated(
    const tab_groups::SavedTabGroup& group,
    tab_groups::TriggerSource source) {
  page_->TabGroupUpdated(CreateMojoTabGroup(group));
}

void TabGroupsOrganizerPageHandler::OnTabGroupRemoved(
    const base::Uuid& sync_id,
    tab_groups::TriggerSource source) {
  page_->TabGroupRemoved(sync_id.AsLowercaseString());
}

void TabGroupsOrganizerPageHandler::OnTabGroupLocalIdChanged(
    const base::Uuid& sync_id,
    const std::optional<tab_groups::LocalTabGroupID>& local_id) {
  if (!tab_group_sync_service_) {
    return;
  }
  std::optional<tab_groups::SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(sync_id);
  if (!group) {
    return;
  }
  if (local_id.has_value()) {
    page_->TabGroupRemoved(tab_groups::LocalTabGroupIDToString(*local_id));
  }
  page_->TabGroupUpdated(CreateMojoTabGroup(*group));
}

void TabGroupsOrganizerPageHandler::OnWillBeDestroyed() {
  tab_group_sync_service_observation_.Reset();
  tab_group_sync_service_ = nullptr;
}

void TabGroupsOrganizerPageHandler::OnTabGroupAdded(
    const tab_groups::TabGroupId& group_id) {
  if (const TabGroup* tab_group = GetUnsavedTabGroup(group_id)) {
    page_->TabGroupAdded(CreateMojoTabGroup(tab_group));
  }
}

void TabGroupsOrganizerPageHandler::OnTabGroupChanged(
    const TabGroupChange& change) {
  if (IsGroupInSyncService(change.group)) {
    return;
  }
  if (change.type == TabGroupChange::kClosed) {
    page_->TabGroupRemoved(tab_groups::LocalTabGroupIDToString(change.group));
    return;
  }
  if (change.type != TabGroupChange::kVisualsChanged ||
      !change.GetVisualsChange()->new_visuals) {
    return;
  }
  if (const TabGroup* tab_group = GetUnsavedTabGroup(change.group)) {
    page_->TabGroupUpdated(CreateMojoTabGroup(tab_group));
  }
}

void TabGroupsOrganizerPageHandler::OnContextMenuClosed() {
  if (on_menu_closed_callback_) {
    std::move(on_menu_closed_callback_).Run();
  }

  if (context_menu_runner_ && context_menu_runner_->IsRunning()) {
    context_menu_runner_->Cancel();
  }
  context_menu_runner_.reset();
}

int TabGroupsOrganizerPageHandler::GetAndIncrementLatestCommandId() {
  return latest_command_id_ += 1;
}

TabStripModel* TabGroupsOrganizerPageHandler::GetTabStripModel() const {
  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_contents_);
  return browser ? browser->GetTabStripModel() : nullptr;
}

const TabGroup* TabGroupsOrganizerPageHandler::GetUnsavedTabGroup(
    const tab_groups::TabGroupId& group_id) const {
  if (IsGroupInSyncService(group_id)) {
    return nullptr;
  }
  TabStripModel* tab_strip_model = GetTabStripModel();
  if (!tab_strip_model || !tab_strip_model->SupportsTabGroups() ||
      tab_strip_model->IsEphemeralTabGroup(group_id) ||
      !tab_strip_model->group_model()->ContainsTabGroup(group_id)) {
    return nullptr;
  }
  const TabGroup* tab_group =
      tab_strip_model->group_model()->GetTabGroup(group_id);
  if (!tab_group || tab_group->IsEmpty()) {
    return nullptr;
  }
  return tab_group;
}

bool TabGroupsOrganizerPageHandler::IsGroupInSyncService(
    const tab_groups::TabGroupId& group_id) const {
  if (!tab_group_sync_service_) {
    return false;
  }
  std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_sync_service_->GetGroup(group_id);
  return saved_group.has_value() && !saved_group->saved_tabs().empty();
}
