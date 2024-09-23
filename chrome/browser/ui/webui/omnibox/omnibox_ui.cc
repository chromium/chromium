// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_page_handler.h"
#include "chrome/browser/ui/webui/version/version_handler.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/omnibox_resources.h"
#include "chrome/grit/omnibox_resources_map.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"

OmniboxUI::OmniboxUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  // Set up the chrome://omnibox/ source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIOmniboxHost);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types parse-html-subset;");

  // Expose version information to client because it is useful in output.
  VersionUI::AddVersionDetailStrings(source);
  source->UseStringsJs();

  source->AddResourcePaths(
      base::make_span(kOmniboxResources, kOmniboxResourcesSize));
  source->SetDefaultResource(IDR_OMNIBOX_OMNIBOX_HTML);
  source->AddResourcePath("ml", IDR_OMNIBOX_ML_ML_HTML);

  source->AddBoolean("isMlUrlScoringEnabled",
                     OmniboxFieldTrial::IsMlUrlScoringEnabled());
}

WEB_UI_CONTROLLER_TYPE_IMPL(OmniboxUI)

OmniboxUI::~OmniboxUI() {}

void OmniboxUI::BindInterface(
    mojo::PendingReceiver<mojom::OmniboxPageHandler> receiver) {
  omnibox_handler_ = std::make_unique<OmniboxPageHandler>(
      Profile::FromWebUI(web_ui()), std::move(receiver));
}
