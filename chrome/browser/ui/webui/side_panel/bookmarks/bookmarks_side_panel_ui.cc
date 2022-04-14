// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_side_panel_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_resources.h"
#include "chrome/grit/side_panel_resources_map.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/style/platform_style.h"

BookmarksSidePanelUI::BookmarksSidePanelUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIBookmarksSidePanelHost);
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"bookmarksTabTitle", IDS_BOOKMARK_MANAGER_TITLE},
      {"bookmarkCopied", IDS_BOOKMARK_MANAGER_TOAST_ITEM_COPIED},
      {"bookmarkDeleted", IDS_BOOKMARK_MANAGER_TOAST_ITEM_DELETED},
      {"bookmarkCreated", IDS_BOOKMARK_SCREEN_READER_CREATED},
      {"bookmarkReordered", IDS_BOOKMARK_SCREEN_READER_REORDERED},
      {"bookmarkMoved", IDS_BOOKMARK_SCREEN_READER_MOVED},
      {"sidePanelTitle", IDS_SIDE_PANEL_TITLE},
      {"tooltipClose", IDS_CLOSE},
      {"tooltipDelete", IDS_DELETE},
  };
  for (const auto& str : kLocalizedStrings)
    webui::AddLocalizedString(source, str.name, str.id);

  source->AddBoolean("useRipples", views::PlatformStyle::kUseRipples);

  Profile* const profile = Profile::FromWebUI(web_ui);
  PrefService* prefs = profile->GetPrefs();
  source->AddBoolean(
      "bookmarksDragAndDropEnabled",

      base::FeatureList::IsEnabled(features::kSidePanelDragAndDrop) &&
          prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled));

  source->AddBoolean("unifiedSidePanel",
                     base::FeatureList::IsEnabled(features::kUnifiedSidePanel));

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
  const int resource = IDR_SIDE_PANEL_BOOKMARKS_BOOKMARKS_HTML;
  webui::SetupWebUIDataSource(
      source, base::make_span(kSidePanelResources, kSidePanelResourcesSize),
      resource);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
}

BookmarksSidePanelUI::~BookmarksSidePanelUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(BookmarksSidePanelUI)

void BookmarksSidePanelUI::BindInterface(
    mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandlerFactory>
        receiver) {
  bookmarks_page_factory_receiver_.reset();
  bookmarks_page_factory_receiver_.Bind(std::move(receiver));
}

void BookmarksSidePanelUI::CreateBookmarksPageHandler(
    mojo::PendingReceiver<side_panel::mojom::BookmarksPageHandler> receiver) {
  bookmarks_page_handler_ =
      std::make_unique<BookmarksPageHandler>(std::move(receiver), this);
}
