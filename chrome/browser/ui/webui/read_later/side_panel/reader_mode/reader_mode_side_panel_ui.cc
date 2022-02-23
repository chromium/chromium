// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/read_later/side_panel/reader_mode/reader_mode_side_panel_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/read_later/side_panel/reader_mode/reader_mode_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/read_later_resources.h"
#include "chrome/grit/read_later_resources_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/style/platform_style.h"

ReaderModeSidePanelUI::ReaderModeSidePanelUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIReaderModeSidePanelHost);
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"readerModeTabTitle", IDS_READER_MODE_TITLE},
  };
  for (const auto& str : kLocalizedStrings)
    webui::AddLocalizedString(source, str.name, str.id);

  webui::SetupWebUIDataSource(
      source, base::make_span(kReadLaterResources, kReadLaterResourcesSize),
      IDR_READ_LATER_SIDE_PANEL_READER_MODE_READER_MODE_HTML);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
}

ReaderModeSidePanelUI::~ReaderModeSidePanelUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ReaderModeSidePanelUI)

void ReaderModeSidePanelUI::BindInterface(
    mojo::PendingReceiver<reader_mode::mojom::PageHandlerFactory> receiver) {
  reader_mode_page_factory_receiver_.reset();
  reader_mode_page_factory_receiver_.Bind(std::move(receiver));
}
void ReaderModeSidePanelUI::CreatePageHandler(
    mojo::PendingRemote<reader_mode::mojom::Page> page,
    mojo::PendingReceiver<reader_mode::mojom::PageHandler> receiver) {
  DCHECK(page);
  reader_mode_page_handler_ = std::make_unique<ReaderModePageHandler>(
      std::move(page), std::move(receiver));
  if (embedder())
    embedder()->ShowUI();
}
