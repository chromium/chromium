// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/default_browser_modal_ui.h"

#include "base/containers/span.h"
#include "chrome/grit/default_browser_modal_resources.h"
#include "chrome/grit/default_browser_modal_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

DefaultBrowserModalUI::DefaultBrowserModalUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui, /*enable_chrome_send=*/false) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIDefaultBrowserModalHost);

  webui::SetupWebUIDataSource(
      source, base::span(kDefaultBrowserModalResources),
      IDR_DEFAULT_BROWSER_MODAL_DEFAULT_BROWSER_MODAL_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(DefaultBrowserModalUI)

DefaultBrowserModalUI::~DefaultBrowserModalUI() = default;
