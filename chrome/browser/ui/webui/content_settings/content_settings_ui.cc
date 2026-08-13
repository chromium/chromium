// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/content_settings/content_settings_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/content_settings/content_settings_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/content_settings_resources.h"
#include "chrome/grit/content_settings_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace content_settings_internals {

ContentSettingsUI::ContentSettingsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIContentSettingsHost);
  webui::SetupWebUIDataSource(source, kContentSettingsResources,
                              IDR_CONTENT_SETTINGS_INDEX_HTML);
}

ContentSettingsUI::~ContentSettingsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ContentSettingsUI)

void ContentSettingsUI::BindInterface(
    mojo::PendingReceiver<mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ContentSettingsUI::CreatePageHandler(
    mojo::PendingReceiver<mojom::PageHandler> receiver) {
  handler_ = std::make_unique<ContentSettingsHandler>(
      Profile::FromBrowserContext(
          web_ui()->GetWebContents()->GetBrowserContext()),
      std::move(receiver));
}

}  // namespace content_settings_internals
