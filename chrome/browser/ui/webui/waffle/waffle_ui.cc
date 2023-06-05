// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/waffle/waffle_ui.h"

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/webui/waffle/waffle_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/waffle_resources.h"
#include "chrome/grit/waffle_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

WaffleUI::WaffleUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true) {
  CHECK(base::FeatureList::IsEnabled(kWaffle));

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIWaffleHost);

  source->AddLocalizedString("title", IDS_WAFFLE_PAGE_TITLE);

  webui::SetupWebUIDataSource(
      source, base::make_span(kWaffleResources, kWaffleResourcesSize),
      IDR_WAFFLE_WAFFLE_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(WaffleUI)

WaffleUI::~WaffleUI() = default;

void WaffleUI::BindInterface(
    mojo::PendingReceiver<waffle::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void WaffleUI::Initialize(base::OnceClosure display_dialog_callback) {
  CHECK(display_dialog_callback);
  display_dialog_callback_ = std::move(display_dialog_callback);
}

void WaffleUI::CreatePageHandler(
    mojo::PendingReceiver<waffle::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<WaffleHandler>(
      std::move(receiver), std::move(display_dialog_callback_));
}
