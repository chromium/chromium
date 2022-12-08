// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes_side_panel_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/side_panel_resources.h"
#include "chrome/grit/side_panel_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

UserNotesSidePanelUI::UserNotesSidePanelUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIUserNotesSidePanelHost);

  const int resource = IDR_SIDE_PANEL_USER_NOTES_USER_NOTES_HTML;
  webui::SetupWebUIDataSource(
      source, base::make_span(kSidePanelResources, kSidePanelResourcesSize),
      resource);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
}

UserNotesSidePanelUI::~UserNotesSidePanelUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(UserNotesSidePanelUI)

void UserNotesSidePanelUI::BindInterface(
    mojo::PendingReceiver<side_panel::mojom::UserNotesPageHandler> receiver) {
  user_notes_page_handler_ = std::make_unique<UserNotesPageHandler>(
      std::move(receiver), Profile::FromWebUI(web_ui()), this);
}
