// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/feature_showcase_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/feature_showcase_resources.h"
#include "chrome/grit/feature_showcase_resources_map.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

FeatureShowcaseUIConfig::FeatureShowcaseUIConfig()
    : content::DefaultWebUIConfig<FeatureShowcaseUI>(
          content::kChromeUIScheme,
          chrome::kChromeUIFeatureShowcaseHost) {}

bool FeatureShowcaseUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(switches::kFirstRunDesktopRevamp);
}

FeatureShowcaseUI::FeatureShowcaseUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIFeatureShowcaseHost);

  webui::SetupWebUIDataSource(source, kFeatureShowcaseResources,
                              IDR_FEATURE_SHOWCASE_FEATURE_SHOWCASE_HTML);

  source->AddString("message", "Hello from Feature Showcase!");
}

FeatureShowcaseUI::~FeatureShowcaseUI() = default;
