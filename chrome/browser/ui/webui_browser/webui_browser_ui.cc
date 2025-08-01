// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_ui.h"

#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/webui_browser_resources.h"
#include "chrome/grit/webui_browser_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

WebUIBrowserUIConfig::WebUIBrowserUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIWebuiBrowserHost) {}

bool WebUIBrowserUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return webui_browser::IsWebUIBrowserEnabled();
}

WebUIBrowserUI::WebUIBrowserUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Set up the chrome://webui-browser source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIWebuiBrowserHost);

  // Add required resources.
  webui::SetupWebUIDataSource(source, kWebuiBrowserResources,
                              IDR_WEBUI_BROWSER_WEBUI_BROWSER_HTML);

  // As a demonstration of passing a variable for JS to use we pass in some
  // a simple message.
  source->AddString("message", "Hello World from C++!");
}

WebUIBrowserUI::~WebUIBrowserUI() = default;
