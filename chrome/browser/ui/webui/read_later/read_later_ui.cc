// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/read_later/read_later_ui.h"

#include <string>
#include <utility>

#include "base/containers/cxx20_erase.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/read_later/reading_list_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/read_later/read_later_page_handler.h"
#include "chrome/browser/ui/webui/read_later/side_panel/bookmarks_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/read_later_resources.h"
#include "chrome/grit/read_later_resources_map.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/style/platform_style.h"

namespace {
void AddLocalizedString(content::WebUIDataSource* source,
                        const std::string& message,
                        int id) {
  std::u16string str = l10n_util::GetStringUTF16(id);
  base::Erase(str, '&');
  source->AddString(message, str);
}
}  // namespace

ReadLaterUI::ReadLaterUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui),
      webui_load_timer_(web_ui->GetWebContents(),
                        "ReadingList.WebUI.LoadDocumentTime",
                        "ReadingList.WebUI.LoadCompletedTime") {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIReadLaterHost);
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"addCurrentTab", IDS_READ_LATER_ADD_CURRENT_TAB},
      {"bookmarksTabTitle", IDS_BOOKMARK_MANAGER_TITLE},
      {"emptyStateAddFromDialogSubheader",
       IDS_READ_LATER_MENU_EMPTY_STATE_ADD_FROM_DIALOG_SUBHEADER},
      {"emptyStateHeader", IDS_READ_LATER_MENU_EMPTY_STATE_HEADER},
      {"emptyStateSubheader", IDS_READ_LATER_MENU_EMPTY_STATE_SUBHEADER},
      {"readHeader", IDS_READ_LATER_MENU_READ_HEADER},
      {"title", IDS_READ_LATER_TITLE},
      {"sidePanelTitle", IDS_SIDE_PANEL_TITLE},
      {"tooltipClose", IDS_CLOSE},
      {"tooltipDelete", IDS_DELETE},
      {"tooltipMarkAsRead", IDS_READ_LATER_MENU_TOOLTIP_MARK_AS_READ},
      {"tooltipMarkAsUnread", IDS_READ_LATER_MENU_TOOLTIP_MARK_AS_UNREAD},
      {"unreadHeader", IDS_READ_LATER_MENU_UNREAD_HEADER},
  };
  for (const auto& str : kLocalizedStrings)
    AddLocalizedString(source, str.name, str.id);

  const bool show_side_panel =
      base::FeatureList::IsEnabled(features::kSidePanel);

  source->AddBoolean(
      "currentPageActionButtonEnabled",
      base::FeatureList::IsEnabled(features::kReadLaterAddFromDialog) ||
          show_side_panel);
  source->AddBoolean("useRipples", views::PlatformStyle::kUseRipples);

  Profile* const profile = Profile::FromWebUI(web_ui);
  PrefService* prefs = profile->GetPrefs();
  source->AddBoolean(
      "bookmarksDragAndDropEnabled",
      show_side_panel &&
          base::FeatureList::IsEnabled(features::kSidePanelDragAndDrop) &&
          prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled));

  ReadingListModel* const reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(profile);
  source->AddBoolean(
      "hasUnseenReadingListEntries",
      reading_list_model->loaded() ? reading_list_model->unseen_size() : false);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
  webui::SetupWebUIDataSource(
      source, base::make_span(kReadLaterResources, kReadLaterResourcesSize),
      show_side_panel ? IDR_READ_LATER_SIDE_PANEL_SIDE_PANEL_HTML
                      : IDR_READ_LATER_READ_LATER_HTML);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
}

ReadLaterUI::~ReadLaterUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ReadLaterUI)

void ReadLaterUI::BindInterface(
    mojo::PendingReceiver<read_later::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ReadLaterUI::CreatePageHandler(
    mojo::PendingRemote<read_later::mojom::Page> page,
    mojo::PendingReceiver<read_later::mojom::PageHandler> receiver) {
  DCHECK(page);
  page_handler_ = std::make_unique<ReadLaterPageHandler>(
      std::move(receiver), std::move(page), this, web_ui());
}

void ReadLaterUI::BindInterface(
    mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandlerFactory>
        receiver) {
  bookmarks_page_factory_receiver_.reset();
  bookmarks_page_factory_receiver_.Bind(std::move(receiver));
}

void ReadLaterUI::CreateBookmarksPageHandler(
    mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver) {
  bookmarks_page_handler_ =
      std::make_unique<BookmarksPageHandler>(std::move(receiver), this);
}

void ReadLaterUI::SetActiveTabURL(const GURL& url) {
  if (page_handler_)
    page_handler_->SetActiveTabURL(url);
}
