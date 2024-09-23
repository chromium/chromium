// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/predictors/predictors_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/predictors/predictors_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/predictors_resources.h"
#include "chrome/grit/predictors_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace {

void CreateAndAddPredictorsUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIPredictorsHost);
  source->AddResourcePaths(
      base::make_span(kPredictorsResources, kPredictorsResourcesSize));
  source->SetDefaultResource(IDR_PREDICTORS_PREDICTORS_HTML);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::RequireTrustedTypesFor,
      "require-trusted-types-for 'script';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types;");
}

}  // namespace

PredictorsUI::PredictorsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  web_ui->AddMessageHandler(std::make_unique<PredictorsHandler>(profile));
  CreateAndAddPredictorsUIHTMLSource(profile);
}
