// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/memories/memories_ui.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/memories/memories_handler.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/memories_resources.h"
#include "chrome/grit/memories_resources_map.h"
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
      {"title", IDS_MEMORIES_PAGE_TITLE},
  };
  source->AddLocalizedStrings(kStrings);

  // TODO(crbug.com/1173908): Replace these with localized strings.
  source->AddString("clearLabel", u"Clear search");
  source->AddString("searchPrompt", u"Search memories");
  source->AddString("memoryTitleDescription",
                    u"Based on previous web activity");
  source->AddString("topVisitsSectionHeader", u"From Chrome History");
  source->AddString("relatedTabGroupsAndBookmarksSectionHeader",
                    u"From tab groups and bookmarks");
  source->AddString("tabGroupTileCaption", u"Recent tab group");
  source->AddString("relatedSearchesSectionHeader", u"Try searching for");

  webui::SetupWebUIDataSource(
      source, base::make_span(kMemoriesResources, kMemoriesResourcesSize),
      IDR_MEMORIES_MEMORIES_HTML);

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
    mojo::PendingReceiver<memories::mojom::PageHandler> pending_page_handler) {
  memories_handler_ = std::make_unique<MemoriesHandler>(
      std::move(pending_page_handler), profile_, web_contents_);
}
