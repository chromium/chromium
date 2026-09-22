// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/organizer_panel/organizer_panel_page_handler.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/browser/web_contents.h"

OrganizerPanelPageHandler::OrganizerPanelPageHandler(
    mojo::PendingReceiver<organizer_panel::mojom::PageHandler> receiver,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)), web_contents_(web_contents) {
  CHECK(web_contents_);
}

OrganizerPanelPageHandler::~OrganizerPanelPageHandler() = default;

void OrganizerPanelPageHandler::ClosePanel() {
  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_contents_);
  CHECK(browser);

  auto* controller = OrganizerPanelController::From(browser);
  CHECK(controller);
  controller->SetOrganizerVisible(false);
}
