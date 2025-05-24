// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_urls/chrome_urls_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_urls/chrome_urls_handler.h"
#include "chrome/common/chrome_features.h"
#include "components/grit/chrome_urls_resources.h"
#include "components/grit/chrome_urls_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/webui_util.h"
#include "url/gurl.h"

namespace {

void CreateAndAddChromeUrlsUIHtmlSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIChromeURLsHost);

  webui::SetupWebUIDataSource(source, kChromeUrlsResources,
                              IDR_CHROME_URLS_CHROME_URLS_HTML);
}

}  // namespace

namespace chrome_urls {

ChromeUrlsUI::ChromeUrlsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui),
      page_factory_receiver_(this),
      profile_(Profile::FromWebUI(web_ui)) {
  CreateAndAddChromeUrlsUIHtmlSource(profile_);
}

WEB_UI_CONTROLLER_TYPE_IMPL(ChromeUrlsUI)

void ChromeUrlsUI::BindInterface(
    mojo::PendingReceiver<chrome_urls::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ChromeUrlsUI::CreatePageHandler(
    mojo::PendingRemote<chrome_urls::mojom::Page> page,
    mojo::PendingReceiver<chrome_urls::mojom::PageHandler> receiver) {
  DCHECK(page);
  page_handler_ = std::make_unique<ChromeUrlsHandler>(
      std::move(receiver), std::move(page), profile_);
}

ChromeUrlsUI::~ChromeUrlsUI() = default;

}  // namespace chrome_urls
