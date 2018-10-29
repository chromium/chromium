// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_page_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

OmniboxUI::OmniboxUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  // Set up the chrome://omnibox/ source.
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIOmniboxHost);
  source->AddResourcePath("omnibox.css", IDR_OMNIBOX_CSS);
  source->AddResourcePath("omnibox_element.js", IDR_OMNIBOX_ELEMENT_JS);
  source->AddResourcePath("omnibox_inputs.js", IDR_OMNIBOX_INPUTS_JS);
  source->AddResourcePath("omnibox_output.js", IDR_OMNIBOX_OUTPUT_JS);
  source->AddResourcePath("omnibox.js", IDR_OMNIBOX_JS);
  source->AddResourcePath("chrome/browser/ui/webui/omnibox/omnibox.mojom.js",
                          IDR_OMNIBOX_MOJO_JS);
  source->SetDefaultResource(IDR_OMNIBOX_HTML);
  source->UseGzip();

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
  AddHandlerToRegistry(base::BindRepeating(&OmniboxUI::BindOmniboxPageHandler,
                                           base::Unretained(this)));
}

OmniboxUI::~OmniboxUI() {}

void OmniboxUI::BindOmniboxPageHandler(
    mojom::OmniboxPageHandlerRequest request) {
  omnibox_handler_.reset(
      new OmniboxPageHandler(Profile::FromWebUI(web_ui()), std::move(request)));
}
