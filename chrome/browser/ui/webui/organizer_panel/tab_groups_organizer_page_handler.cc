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
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_tabs_menu_model.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/rect.h"
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
}

TabGroupsOrganizerPageHandler::~TabGroupsOrganizerPageHandler() {
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
    }
  }
}

void TabGroupsOrganizerPageHandler::ShowContextMenu(
    const std::string& group_id,
    const gfx::Rect& anchor_rect,
    ShowContextMenuCallback callback) {
  // If the menu was already open, close it.
  OnContextMenuClosed();

  on_menu_closed_callback_ = std::move(callback);

  const base::Uuid uuid = base::Uuid::ParseLowercase(group_id);
  if (!tab_group_sync_service_ || !uuid.is_valid()) {
    OnContextMenuClosed();
    return;
  }

  const std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_sync_service_->GetGroup(uuid);
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
  page_->TabGroupUpdated(CreateMojoTabGroup(*group));
}

void TabGroupsOrganizerPageHandler::OnWillBeDestroyed() {
  tab_group_sync_service_observation_.Reset();
  tab_group_sync_service_ = nullptr;
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
