// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/intro_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/webui/intro/intro_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/intro_resources.h"
#include "chrome/grit/intro_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {}  // namespace

IntroUI::IntroUI(content::WebUI* web_ui) : content::WebUIController(web_ui) {
  DCHECK(base::FeatureList::IsEnabled(kForYouFre));
  web_ui->AddMessageHandler(std::make_unique<IntroHandler>());

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIIntroHost);

  webui::SetupWebUIDataSource(
      source, base::make_span(kIntroResources, kIntroResourcesSize),
      IDR_INTRO_INTRO_HTML);

  source->AddString("message", "Under construction.");
}

IntroUI::~IntroUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(IntroUI)
