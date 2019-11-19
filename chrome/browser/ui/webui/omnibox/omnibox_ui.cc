// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_page_handler.h"
#include "chrome/browser/ui/webui/version_handler.h"
#include "chrome/browser/ui/webui/version_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/omnibox_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

OmniboxUI::OmniboxUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  // Set up the chrome://omnibox/ source.
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIOmniboxHost);

  // Expose version information to client because it is useful in output.
  VersionUI::AddVersionDetailStrings(source);
  source->UseStringsJs();

  source->AddResourcePath("omnibox.css", IDR_OMNIBOX_CSS);
  source->AddResourcePath("omnibox_input.css", IDR_OMNIBOX_INPUT_CSS);
  source->AddResourcePath("output_results_group.css",
                          IDR_OUTPUT_RESULTS_GROUP_CSS);
  source->AddResourcePath("omnibox_output_column_widths.css",
                          IDR_OMNIBOX_OUTPUT_COLUMN_WIDTHS_CSS);
  source->AddResourcePath("omnibox_element.js", IDR_OMNIBOX_ELEMENT_JS);
  source->AddResourcePath("omnibox_input.js", IDR_OMNIBOX_INPUT_JS);
  source->AddResourcePath("omnibox_output.js", IDR_OMNIBOX_OUTPUT_JS);
  source->AddResourcePath("omnibox.js", IDR_OMNIBOX_JS);
  source->AddResourcePath(
      "chrome/browser/ui/webui/omnibox/omnibox.mojom-lite.js",
      IDR_OMNIBOX_MOJO_JS);
  source->SetDefaultResource(IDR_OMNIBOX_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
  AddHandlerToRegistry(base::BindRepeating(&OmniboxUI::BindOmniboxPageHandler,
                                           base::Unretained(this)));
  web_ui->AddMessageHandler(std::make_unique<VersionHandler>());
}

OmniboxUI::~OmniboxUI() {}

void OmniboxUI::BindOmniboxPageHandler(
    mojo::PendingReceiver<mojom::OmniboxPageHandler> receiver) {
  omnibox_handler_ = std::make_unique<OmniboxPageHandler>(
      Profile::FromWebUI(web_ui()), std::move(receiver));
}
