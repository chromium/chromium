// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_ui.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_handler.h"
#include "chrome/grit/tab_strip_internals_resources.h"
#include "chrome/grit/tab_strip_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

TabStripInternalsUI::TabStripInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/false) {
  // Setup up the chrome://tab-strip-internals source
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUITabStripInternalsHost);

  // Add required resources
  webui::SetupWebUIDataSource(source, kTabStripInternalsResources,
                              IDR_TAB_STRIP_INTERNALS_TAB_STRIP_INTERNALS_HTML);
}

TabStripInternalsUI::~TabStripInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(TabStripInternalsUI)

bool TabStripInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return DefaultInternalWebUIConfig::IsWebUIEnabled(browser_context) &&
         base::FeatureList::IsEnabled(tabs::kDebugUITabStrip);
}

void TabStripInternalsUI::BindInterface(
    mojo::PendingReceiver<tab_strip_internals::mojom::PageHandlerFactory>
        receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void TabStripInternalsUI::CreatePageHandler(
    mojo::PendingRemote<tab_strip_internals::mojom::Page> page,
    mojo::PendingReceiver<tab_strip_internals::mojom::PageHandler> receiver) {
  CHECK(page);
  content::WebContents* web_contents = web_ui()->GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  CHECK(profile);
  page_handler_ = std::make_unique<TabStripInternalsPageHandler>(
      profile, std::move(receiver), std::move(page));
}
