// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

#if BUILDFLAG(ENABLE_TAB_SEARCH)
#include "chrome/grit/tab_search_resources.h"
#include "chrome/grit/tab_search_resources_map.h"
#endif  // BUILDFLAG(ENABLE_TAB_SEARCH)

#if BUILDFLAG(ENABLE_TAB_SEARCH)
namespace {
constexpr char kGeneratedPath[] =
    "@out_folder@/gen/chrome/browser/resources/tab_search/";
}
#endif  // BUILDFLAG(ENABLE_TAB_SEARCH)

TabSearchUI::TabSearchUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui,
                              true /* Needed for webui browser tests */),
      webui_load_timer_(web_ui->GetWebContents(),
                        "Tabs.TabSearch.WebUI.LoadDocumentTime",
                        "Tabs.TabSearch.WebUI.LoadCompletedTime") {
#if BUILDFLAG(ENABLE_TAB_SEARCH)
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUITabSearchHost);
  source->AddLocalizedString("close", IDS_CLOSE);
  source->AddResourcePath("tab_search.mojom-lite.js",
                          IDR_TAB_SEARCH_MOJO_LITE_JS);
  source->AddResourcePath("fuse.js", IDR_FUSE_JS);
  webui::SetupWebUIDataSource(
      source, base::make_span(kTabSearchResources, kTabSearchResourcesSize),
      kGeneratedPath, IDR_TAB_SEARCH_PAGE_HTML);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
#endif  // BUILDFLAG(ENABLE_TAB_SEARCH)
}

TabSearchUI::~TabSearchUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(TabSearchUI)

void TabSearchUI::BindInterface(
    mojo::PendingReceiver<tab_search::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void TabSearchUI::AddShowUICallback(base::OnceClosure callback) {
  show_ui_callback_ = std::move(callback);
}

void TabSearchUI::ShowUI() {
  if (show_ui_callback_)
    std::move(show_ui_callback_).Run();
}

void TabSearchUI::CreatePageHandler(
    mojo::PendingRemote<tab_search::mojom::Page> page,
    mojo::PendingReceiver<tab_search::mojom::PageHandler> receiver) {
  DCHECK(page);
  page_handler_ = std::make_unique<TabSearchPageHandler>(std::move(receiver),
                                                         std::move(page), this);
}
