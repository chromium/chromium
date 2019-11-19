// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/new_tab_page_resources.h"
#include "chrome/grit/new_tab_page_resources_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_data_source.h"

using content::BrowserContext;
using content::WebContents;

namespace {

content::WebUIDataSource* CreateNewTabPageUiHtmlSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUINewTabPageHost);

  static constexpr LocalizedString kStrings[] = {
      {"title", IDS_NEW_TAB_TITLE},
  };
  AddLocalizedStringsBulk(source, kStrings, base::size(kStrings));

  source->AddResourcePath("new_tab_page.mojom-lite.js",
                          IDR_NEW_TAB_PAGE_MOJO_LITE_JS);

  std::string generated_path =
      "@out_folder@/gen/chrome/browser/resources/new_tab_page/";
  for (size_t i = 0; i < kNewTabPageResourcesSize; ++i) {
    base::StringPiece path = kNewTabPageResources[i].name;
    if (path.rfind(generated_path, 0) == 0) {
      path = path.substr(generated_path.length());
    }
    source->AddResourcePath(path, kNewTabPageResources[i].value);
  }

  source->SetDefaultResource(IDR_NEW_TAB_PAGE_NEW_TAB_PAGE_HTML);
  source->UseStringsJs();

  return source;
}

}  // namespace

NewTabPageUI::NewTabPageUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true), page_factory_receiver_(this) {
  Profile* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource* source = CreateNewTabPageUiHtmlSource();
  ManagedUIHandler::Initialize(web_ui, source);
  content::WebUIDataSource::Add(profile, source);

  AddHandlerToRegistry(base::BindRepeating(
      &NewTabPageUI::BindPageHandlerFactory, base::Unretained(this)));
}

NewTabPageUI::~NewTabPageUI() = default;

void NewTabPageUI::BindPageHandlerFactory(
    mojo::PendingReceiver<new_tab_page::mojom::PageHandlerFactory>
        pending_receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }

  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageUI::CreatePageHandler(
    mojo::PendingRemote<new_tab_page::mojom::Page> pending_page,
    mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
        pending_page_handler) {
  DCHECK(pending_page.is_valid());
  page_handler_ = std::make_unique<NewTabPageHandler>(
      std::move(pending_page_handler), std::move(pending_page));
}

// static
bool NewTabPageUI::IsNewTabPageOrigin(const GURL& url) {
  return url.GetOrigin() == GURL(chrome::kChromeUINewTabPageURL).GetOrigin();
}
