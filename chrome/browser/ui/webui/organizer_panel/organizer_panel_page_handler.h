// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/organizer_panel/organizer_panel.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class WebContents;
}  // namespace content

class OrganizerPanelPageHandler : public organizer_panel::mojom::PageHandler {
 public:
  OrganizerPanelPageHandler(
      mojo::PendingReceiver<organizer_panel::mojom::PageHandler> receiver,
      content::WebContents* web_contents);
  OrganizerPanelPageHandler(const OrganizerPanelPageHandler&) = delete;
  OrganizerPanelPageHandler& operator=(const OrganizerPanelPageHandler&) =
      delete;
  ~OrganizerPanelPageHandler() override;

  // organizer_panel::mojom::PageHandler:
  void ClosePanel() override;

 private:
  mojo::Receiver<organizer_panel::mojom::PageHandler> receiver_;
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_ORGANIZER_PANEL_ORGANIZER_PANEL_PAGE_HANDLER_H_
