// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_clusters/memories_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/history_clusters/history_clusters_handler.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/history_clusters_resources.h"
#include "chrome/grit/history_clusters_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

content::WebUIDataSource* CreateAndSetupWebUIDataSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIMemoriesHost);

  static constexpr webui::LocalizedString kStrings[] = {
      {"actionMenuDescription", IDS_HISTORY_ACTION_MENU_DESCRIPTION},
      {"bookmarked", IDS_HISTORY_ENTRY_BOOKMARKED},
      {"cancel", IDS_CANCEL},
      {"deleteConfirm", IDS_HISTORY_DELETE_PRIOR_VISITS_CONFIRM_BUTTON},
      {"deleteWarning", IDS_HISTORY_DELETE_PRIOR_VISITS_WARNING},
      {"removeFromHistory", IDS_HISTORY_REMOVE_PAGE},
      {"removeSelected", IDS_HISTORY_REMOVE_SELECTED_ITEMS},
      {"title", IDS_MEMORIES_PAGE_TITLE},
  };
  source->AddLocalizedStrings(kStrings);

  // TODO(crbug.com/1173908): Replace these with localized strings.
  source->AddString("clearLabel", u"Clear search");
  source->AddString("headerTitle", u"Based on web activity related to \"$1\"");
  source->AddString("relatedSearchesLabel", u"Related:");
  source->AddString("removeAllFromHistory", u"Remove all from history");
  source->AddString("savedInTabGroup", u"Saved in tab group");
  source->AddString("searchPrompt", u"Search clusters");
  source->AddString("toggleButtonLabelLess", u"Show less");
  source->AddString("toggleButtonLabelMore", u"Show more");

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kHistoryClustersResources, kHistoryClustersResourcesSize),
      IDR_HISTORY_CLUSTERS_HISTORY_CLUSTERS_HTML);

  content::URLDataSource::Add(profile,
                              std::make_unique<SanitizedImageSource>(profile));
  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  return source;
}

}  // namespace

MemoriesUI::MemoriesUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      profile_(Profile::FromWebUI(web_ui)),
      web_contents_(web_ui->GetWebContents()) {
  DCHECK(profile_);
  DCHECK(web_contents_);

  auto* source = CreateAndSetupWebUIDataSource(profile_);
  content::WebUIDataSource::Add(profile_, source);
}

WEB_UI_CONTROLLER_TYPE_IMPL(MemoriesUI)

MemoriesUI::~MemoriesUI() = default;

void MemoriesUI::BindInterface(
    mojo::PendingReceiver<history_clusters::mojom::PageHandler>
        pending_page_handler) {
  history_clusters_handler_ = std::make_unique<HistoryClustersHandler>(
      std::move(pending_page_handler), profile_, web_contents_);
}
