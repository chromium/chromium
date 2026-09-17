// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_TAB_GROUPS_ORGANIZER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_TAB_GROUPS_ORGANIZER_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/organizer_panel/tab_groups.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace tab_groups {
class TabGroupSyncService;
}

class TabGroupsOrganizerPageHandler
    : public organizer_panel::mojom::TabGroupsOrganizerPageHandler {
 public:
  TabGroupsOrganizerPageHandler(
      mojo::PendingReceiver<
          organizer_panel::mojom::TabGroupsOrganizerPageHandler> receiver,
      Profile* profile);
  TabGroupsOrganizerPageHandler(const TabGroupsOrganizerPageHandler&) = delete;
  TabGroupsOrganizerPageHandler& operator=(
      const TabGroupsOrganizerPageHandler&) = delete;
  ~TabGroupsOrganizerPageHandler() override;

  // organizer_panel::mojom::TabGroupsOrganizerPageHandler:
  void GetTabGroups(GetTabGroupsCallback callback) override;

 private:
  mojo::Receiver<organizer_panel::mojom::TabGroupsOrganizerPageHandler>
      receiver_;
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_TAB_GROUPS_ORGANIZER_PAGE_HANDLER_H_
