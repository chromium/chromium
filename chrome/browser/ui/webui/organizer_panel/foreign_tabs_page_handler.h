// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_FOREIGN_TABS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_FOREIGN_TABS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/organizer_panel/foreign_tabs.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

class ForeignTabsPageHandler
    : public organizer_panel::mojom::ForeignTabsPageHandler {
 public:
  ForeignTabsPageHandler(
      mojo::PendingReceiver<organizer_panel::mojom::ForeignTabsPageHandler>
          receiver,
      content::WebContents* web_contents);
  ForeignTabsPageHandler(
      mojo::PendingReceiver<organizer_panel::mojom::ForeignTabsPageHandler>
          receiver,
      Profile* profile);
  ForeignTabsPageHandler(const ForeignTabsPageHandler&) = delete;
  ForeignTabsPageHandler& operator=(const ForeignTabsPageHandler&) = delete;
  ~ForeignTabsPageHandler() override;

  // organizer_panel::mojom::ForeignTabsPageHandler:
  void GetForeignTabs(GetForeignTabsCallback callback) override;

 private:
  mojo::Receiver<organizer_panel::mojom::ForeignTabsPageHandler> receiver_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_FOREIGN_TABS_PAGE_HANDLER_H_
