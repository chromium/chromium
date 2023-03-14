// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes_side_panel_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_shared_resources.h"
#include "chrome/grit/side_panel_shared_resources_map.h"
#include "chrome/grit/side_panel_user_notes_resources.h"
#include "chrome/grit/side_panel_user_notes_resources_map.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_notes/user_notes_prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/ui_base_features.h"

UserNotesSidePanelUI::UserNotesSidePanelUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui, true) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIUserNotesSidePanelHost);
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"add", IDS_ADD},
      {"addANote", IDS_ADD_NEW_USER_NOTE_PLACEHOLDER_TEXT},
      {"allNotes", IDS_ALL_NOTES},
      {"cancel", IDS_CANCEL},
      {"currentTab", IDS_USER_NOTES_CURRENT_TAB_HEADER},
      {"delete", IDS_DELETE},
      {"edit", IDS_EDIT},
      {"emptyTitle", IDS_USER_NOTES_EMPTY_STATE_TITLE},
      {"emptyBody", IDS_USER_NOTES_EMPTY_STATE_BODY},
      {"emptyTitleGuest", IDS_USER_NOTES_EMPTY_STATE_TITLE_GUEST},
      {"emptyBodyGuest", IDS_USER_NOTES_EMPTY_STATE_BODY_GUEST},
      {"openInNewTab", IDS_USER_NOTES_MENU_OPEN_IN_NEW_TAB},
      {"openInNewWindow", IDS_USER_NOTES_MENU_OPEN_IN_NEW_WINDOW},
      {"openInIncognitoWindow", IDS_USER_NOTES_MENU_OPEN_IN_INCOGNITO},
      {"sortByType", IDS_BOOKMARKS_SORT_BY_TYPE},
      {"sortNewest", IDS_BOOKMARKS_SORT_NEWEST},
      {"sortMenuAriaLabel", IDS_USER_NOTES_SORT_MENU_A11Y_LABEL},
      {"sortOldest", IDS_BOOKMARKS_SORT_OLDEST},
      {"title", IDS_USER_NOTE_TITLE},
      {"tooltipClose", IDS_CLOSE},
  };
  for (const auto& str : kLocalizedStrings) {
    webui::AddLocalizedString(source, str.name, str.id);
  }

  Profile* const profile = Profile::FromWebUI(web_ui);
  PrefService* pref_service = profile->GetPrefs();
  if (pref_service) {
    source->AddBoolean("sortByNewest",
                       pref_service->GetBoolean(prefs::kUserNotesSortByNewest));
  }
  source->AddBoolean("guestMode", profile->IsGuestSession());

  source->AddString(
      "chromeRefresh2023Attribute",
      features::IsChromeRefresh2023() ? "chrome-refresh-2023" : "");

  webui::SetupWebUIDataSource(source,
                              base::make_span(kSidePanelUserNotesResources,
                                              kSidePanelUserNotesResourcesSize),
                              IDR_SIDE_PANEL_USER_NOTES_USER_NOTES_HTML);
  source->AddResourcePaths(base::make_span(kSidePanelSharedResources,
                                           kSidePanelSharedResourcesSize));

  // Add a handler to provide pluralized string.
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString("notesCount", IDS_NOTES_COUNT);
  web_ui->AddMessageHandler(std::move(plural_string_handler));
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
      browser_, start_creation_flow_, this);
  start_creation_flow_ = false;
}

base::WeakPtr<UserNotesSidePanelUI> UserNotesSidePanelUI::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void UserNotesSidePanelUI::StartNoteCreation(bool wait_for_tab_change) {
  if (user_notes_page_handler_) {
    user_notes_page_handler_->StartNoteCreation(wait_for_tab_change);
  } else {
    start_creation_flow_ = true;
  }
}
