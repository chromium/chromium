// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_TAB_GROUPS_ORGANIZER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_TAB_GROUPS_ORGANIZER_PAGE_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/uuid.h"
#include "chrome/browser/ui/webui/organizer_panel/tab_groups.mojom.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Rect;
}

namespace tab_groups {
class STGTabsMenuModel;
}  // namespace tab_groups

namespace views {
class MenuRunner;
}

class TabGroupsOrganizerPageHandler
    : public organizer_panel::mojom::TabGroupsOrganizerPageHandler,
      public tab_groups::TabGroupSyncService::Observer {
 public:
  TabGroupsOrganizerPageHandler(
      mojo::PendingReceiver<
          organizer_panel::mojom::TabGroupsOrganizerPageHandler> receiver,
      mojo::PendingRemote<organizer_panel::mojom::TabGroupsOrganizerPage> page,
      content::WebContents* web_contents);
  TabGroupsOrganizerPageHandler(const TabGroupsOrganizerPageHandler&) = delete;
  TabGroupsOrganizerPageHandler& operator=(
      const TabGroupsOrganizerPageHandler&) = delete;
  ~TabGroupsOrganizerPageHandler() override;

  // organizer_panel::mojom::TabGroupsOrganizerPageHandler:
  void GetTabGroups(GetTabGroupsCallback callback) override;
  void OpenTabGroup(const std::string& id) override;
  void ShowContextMenu(const std::string& group_id,
                       const gfx::Rect& anchor_rect,
                       ShowContextMenuCallback callback) override;

  bool IsContextMenuRunningForTesting() const;

  // tab_groups::TabGroupSyncService::Observer:
  void OnTabGroupAdded(const tab_groups::SavedTabGroup& group,
                       tab_groups::TriggerSource source) override;
  void OnTabGroupUpdated(const tab_groups::SavedTabGroup& group,
                         tab_groups::TriggerSource source) override;
  void OnTabGroupRemoved(const base::Uuid& sync_id,
                         tab_groups::TriggerSource source) override;
  void OnTabGroupLocalIdChanged(
      const base::Uuid& sync_id,
      const std::optional<tab_groups::LocalTabGroupID>& local_id) override;
  void OnWillBeDestroyed() override;

 private:
  void OnContextMenuClosed();
  int GetAndIncrementLatestCommandId();

  mojo::Receiver<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      receiver_;
  mojo::Remote<organizer_panel::mojom::TabGroupsOrganizerPage> page_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;
  base::ScopedObservation<tab_groups::TabGroupSyncService,
                          tab_groups::TabGroupSyncService::Observer>
      tab_group_sync_service_observation_{this};
  int latest_command_id_ = 0;
  ShowContextMenuCallback on_menu_closed_callback_;
  std::unique_ptr<tab_groups::STGTabsMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  base::WeakPtrFactory<TabGroupsOrganizerPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_TAB_GROUPS_ORGANIZER_PAGE_HANDLER_H_
