// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/browsing_topics/browsing_topics_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/browsing_topics/browsing_topics_internals_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browsing_topics_internals_resources.h"
#include "chrome/grit/browsing_topics_internals_resources_map.h"
#include "components/browsing_topics/mojom/browsing_topics_internals.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

BrowsingTopicsInternalsUI::BrowsingTopicsInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true) {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIBrowsingTopicsInternalsHost);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kBrowsingTopicsInternalsResources,
                      kBrowsingTopicsInternalsResourcesSize),
      IDR_BROWSING_TOPICS_INTERNALS_BROWSING_TOPICS_INTERNALS_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
}

BrowsingTopicsInternalsUI::~BrowsingTopicsInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(BrowsingTopicsInternalsUI)

void BrowsingTopicsInternalsUI::BindInterface(
    mojo::PendingReceiver<browsing_topics::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<BrowsingTopicsInternalsPageHandler>(
      Profile::FromBrowserContext(
          web_ui()->GetWebContents()->GetBrowserContext()),
      std::move(receiver));
}
