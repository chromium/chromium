// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_reading_list_resources.h"
#include "chrome/grit/side_panel_reading_list_resources_map.h"
#include "chrome/grit/side_panel_shared_resources.h"
#include "chrome/grit/side_panel_shared_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/style/platform_style.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

ReadingListUI::ReadingListUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui),
      webui_load_timer_(web_ui->GetWebContents(),
                        "ReadingList.WebUI.LoadDocumentTime",
                        "ReadingList.WebUI.LoadCompletedTime") {
  Profile* const profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIReadLaterHost);
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"addCurrentTab", IDS_READ_LATER_ADD_CURRENT_TAB},
      {"emptyStateAddFromDialogSubheader",
       IDS_READ_LATER_MENU_EMPTY_STATE_ADD_FROM_DIALOG_SUBHEADER},
      {"emptyStateHeader", IDS_READ_LATER_MENU_EMPTY_STATE_HEADER},
      {"emptyStateSubheader", IDS_READ_LATER_MENU_EMPTY_STATE_SUBHEADER},
      {"markCurrentTabAsRead", IDS_READ_LATER_MARK_CURRENT_TAB_READ},
      {"readHeader", IDS_READ_LATER_MENU_READ_HEADER},
      {"title", IDS_READ_LATER_TITLE},
      {"tooltipDelete", IDS_DELETE},
      {"tooltipMarkAsRead", IDS_READ_LATER_MENU_TOOLTIP_MARK_AS_READ},
      {"tooltipMarkAsUnread", IDS_READ_LATER_MENU_TOOLTIP_MARK_AS_UNREAD},
      {"unreadHeader", IDS_READ_LATER_MENU_UNREAD_HEADER},
      {"cancelA11yLabel", IDS_CANCEL},
  };
  for (const auto& str : kLocalizedStrings)
    webui::AddLocalizedString(source, str.name, str.id);

  source->AddBoolean("useRipples", views::PlatformStyle::kUseRipples);

  ReadingListModel* const reading_list_model =
      ReadingListModelFactory::GetForBrowserContext(profile);
  source->AddBoolean(
      "hasUnseenReadingListEntries",
      reading_list_model->loaded() ? reading_list_model->unseen_size() : false);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
  webui::SetupWebUIDataSource(
      source,
      base::make_span(kSidePanelReadingListResources,
                      kSidePanelReadingListResourcesSize),
      IDR_SIDE_PANEL_READING_LIST_READING_LIST_HTML);
  source->AddResourcePaths(base::make_span(kSidePanelSharedResources,
                                           kSidePanelSharedResourcesSize));
}

ReadingListUI::~ReadingListUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ReadingListUI)

void ReadingListUI::BindInterface(
    mojo::PendingReceiver<reading_list::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ReadingListUI::CreatePageHandler(
    mojo::PendingRemote<reading_list::mojom::Page> page,
    mojo::PendingReceiver<reading_list::mojom::PageHandler> receiver) {
  DCHECK(page);
  page_handler_ = std::make_unique<ReadingListPageHandler>(
      std::move(receiver), std::move(page), this, web_ui());
}

void ReadingListUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void ReadingListUI::BindInterface(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        pending_receiver) {
  if (help_bubble_handler_factory_receiver_.is_bound())
    help_bubble_handler_factory_receiver_.reset();
  help_bubble_handler_factory_receiver_.Bind(std::move(pending_receiver));
}

void ReadingListUI::CreateHelpBubbleHandler(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler) {
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(handler), std::move(client), this,
      std::vector<ui::ElementIdentifier>{
          kAddCurrentTabToReadingListElementId,
          kSidePanelReadingListUnreadElementId,
      });
}

void ReadingListUI::SetActiveTabURL(const GURL& url) {
  if (page_handler_)
    page_handler_->SetActiveTabURL(url);
}
