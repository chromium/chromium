// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/predictors/predictors_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/predictors/predictors_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

content::WebUIDataSource* CreatePredictorsUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIPredictorsHost);
  source->AddResourcePath("autocomplete_action_predictor.js",
                          IDR_PREDICTORS_AUTOCOMPLETE_ACTION_PREDICTOR_JS);
  source->AddResourcePath("predictors.js", IDR_PREDICTORS_JS);
  source->AddResourcePath("resource_prefetch_predictor.js",
                          IDR_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_JS);
  source->SetDefaultResource(IDR_PREDICTORS_HTML);
  return source;
}

}  // namespace

PredictorsUI::PredictorsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  web_ui->AddMessageHandler(std::make_unique<PredictorsHandler>(profile));
  content::WebUIDataSource::Add(profile, CreatePredictorsUIHTMLSource());
}
