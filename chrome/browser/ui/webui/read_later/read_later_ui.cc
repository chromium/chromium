// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/read_later/read_later_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/read_later/read_later_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/read_later_resources.h"
#include "chrome/grit/read_later_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
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
      {"emptyStateHeader", IDS_READ_LATER_MENU_EMPTY_STATE_HEADER},
      {"emptyStateSubheader", IDS_READ_LATER_MENU_EMPTY_STATE_SUBHEADER},
      {"readHeader", IDS_READ_LATER_MENU_READ_HEADER},
      {"title", IDS_READ_LATER_TITLE},
      {"tooltipClose", IDS_CLOSE},
      {"tooltipDelete", IDS_DELETE},
      {"tooltipMarkAsRead", IDS_READ_LATER_MENU_TOOLTIP_MARK_AS_READ},
      {"tooltipMarkAsUnread", IDS_READ_LATER_MENU_TOOLTIP_MARK_AS_UNREAD},
      {"unreadHeader", IDS_READ_LATER_MENU_UNREAD_HEADER},
  };
  for (const auto& str : kLocalizedStrings)
    AddLocalizedString(source, str.name, str.id);

  source->AddBoolean("useRipples", views::PlatformStyle::kUseRipples);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  const bool show_side_panel_prototype =
      base::FeatureList::IsEnabled(features::kSidePanel) &&
      base::FeatureList::IsEnabled(features::kSidePanelPrototype);
  webui::SetupWebUIDataSource(
      source, base::make_span(kReadLaterResources, kReadLaterResourcesSize),
      show_side_panel_prototype ? IDR_READ_LATER_SIDE_PANEL_SIDE_PANEL_HTML
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
