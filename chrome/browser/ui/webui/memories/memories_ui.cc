// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/memories/memories_ui.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/memories/memories_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/memories_resources.h"
#include "chrome/grit/memories_resources_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

content::WebUIDataSource* CreateAndSetupWebUIDataSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIMemoriesHost);

  static constexpr webui::LocalizedString kStrings[] = {
      {"title", IDS_MEMORIES_PAGE_TITLE},
  };
  source->AddLocalizedStrings(kStrings);

  webui::SetupWebUIDataSource(
      source, base::make_span(kMemoriesResources, kMemoriesResourcesSize),
      IDR_MEMORIES_MEMORIES_HTML);

  return source;
}

}  // namespace

MemoriesUI::MemoriesUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      profile_(Profile::FromWebUI(web_ui)),
      web_contents_(web_ui->GetWebContents()) {
  DCHECK(profile_);
  DCHECK(web_contents_);

  auto* source = CreateAndSetupWebUIDataSource();
  content::WebUIDataSource::Add(profile_, source);
}

WEB_UI_CONTROLLER_TYPE_IMPL(MemoriesUI)

MemoriesUI::~MemoriesUI() = default;

void MemoriesUI::BindInterface(
    mojo::PendingReceiver<memories::mojom::PageHandler> pending_page_handler) {
  memories_handler_ = std::make_unique<MemoriesHandler>(
      std::move(pending_page_handler), profile_, web_contents_);
}
