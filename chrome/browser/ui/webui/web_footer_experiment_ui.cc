// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/web_footer_experiment_ui.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui_data_source.h"

WebFooterExperimentUI::WebFooterExperimentUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIWebFooterExperimentHost);
  source->AddResourcePath("index.html", IDR_WEB_FOOTER_EXPERIMENT_HTML);
  source->SetDefaultResource(IDR_WEB_FOOTER_EXPERIMENT_HTML);
  ManagedUIHandler::Initialize(web_ui, source);
  content::WebUIDataSource::Add(profile, source);
}

WebFooterExperimentUI::~WebFooterExperimentUI() = default;
