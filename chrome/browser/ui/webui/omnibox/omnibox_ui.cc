// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_page_handler.h"
#include "chrome/browser/ui/webui/version_handler.h"
#include "chrome/browser/ui/webui/version_ui.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/omnibox_resources.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/omnibox/omnibox_popup_handler.h"
#endif

OmniboxUI::OmniboxUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  // Set up the chrome://omnibox/ source.
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIOmniboxHost);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types parse-html-subset;");

  // Expose version information to client because it is useful in output.
  VersionUI::AddVersionDetailStrings(source);
  source->UseStringsJs();

  static constexpr webui::ResourcePath kResources[] = {
      {"omnibox.css", IDR_OMNIBOX_CSS},
      {"omnibox_input.css", IDR_OMNIBOX_INPUT_CSS},
      {"output_results_group.css", IDR_OUTPUT_RESULTS_GROUP_CSS},
      {"omnibox_output_column_widths.css",
       IDR_OMNIBOX_OUTPUT_COLUMN_WIDTHS_CSS},
      {"omnibox_element.js", IDR_OMNIBOX_ELEMENT_JS},
      {"omnibox_input.js", IDR_OMNIBOX_INPUT_JS},
      {"omnibox_output.js", IDR_OMNIBOX_OUTPUT_JS},
      {"omnibox.js", IDR_OMNIBOX_JS},
      {"chrome/browser/ui/webui/omnibox/omnibox.mojom-webui.js",
       IDR_OMNIBOX_MOJO_JS},
  };
  webui::AddResourcePathsBulk(source, kResources);

  source->SetDefaultResource(IDR_OMNIBOX_HTML);

#if !defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup)) {
    source->AddResourcePath("omnibox_popup.js", IDR_OMNIBOX_POPUP_JS);
    source->AddResourcePath("omnibox_popup.html", IDR_OMNIBOX_POPUP_HTML);

    popup_handler_ = std::make_unique<OmniboxPopupHandler>();
  }
#endif

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
  web_ui->AddMessageHandler(std::make_unique<VersionHandler>());
}

WEB_UI_CONTROLLER_TYPE_IMPL(OmniboxUI)

OmniboxUI::~OmniboxUI() {}

void OmniboxUI::BindInterface(
    mojo::PendingReceiver<mojom::OmniboxPageHandler> receiver) {
  omnibox_handler_ = std::make_unique<OmniboxPageHandler>(
      Profile::FromWebUI(web_ui()), std::move(receiver));
}
