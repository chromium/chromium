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
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_user_notes_resources.h"
#include "chrome/grit/side_panel_user_notes_resources_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/ui_base_features.h"

UserNotesSidePanelUI::UserNotesSidePanelUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIUserNotesSidePanelHost);
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"add", IDS_ADD},
      {"addANote", IDS_ADD_NEW_USER_NOTE_PLACEHOLDER_TEXT},
      {"cancel", IDS_CANCEL},
      {"delete", IDS_DELETE},
      {"edit", IDS_EDIT},
      {"title", IDS_USER_NOTE_TITLE},
      {"tooltipClose", IDS_CLOSE},
  };
  for (const auto& str : kLocalizedStrings) {
    webui::AddLocalizedString(source, str.name, str.id);
  }

  source->AddString(
      "chromeRefresh2023Attribute",
      features::IsChromeRefresh2023() ? "chrome-refresh-2023" : "");

  webui::SetupWebUIDataSource(source,
                              base::make_span(kSidePanelUserNotesResources,
                                              kSidePanelUserNotesResourcesSize),
                              IDR_SIDE_PANEL_USER_NOTES_USER_NOTES_HTML);
}

UserNotesSidePanelUI::~UserNotesSidePanelUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(UserNotesSidePanelUI)

void UserNotesSidePanelUI::BindInterface(
    mojo::PendingReceiver<side_panel::mojom::UserNotesPageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void UserNotesSidePanelUI::CreatePageHandler(
    mojo::PendingRemote<side_panel::mojom::UserNotesPage> page,
    mojo::PendingReceiver<side_panel::mojom::UserNotesPageHandler> receiver) {
  DCHECK(page);
  if (!browser_) {
    return;
  }
  user_notes_page_handler_ = std::make_unique<UserNotesPageHandler>(
      std::move(receiver), std::move(page), Profile::FromWebUI(web_ui()),
      browser_, this);
}
