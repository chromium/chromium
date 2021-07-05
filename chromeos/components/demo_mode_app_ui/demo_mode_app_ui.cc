// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/demo_mode_app_ui/demo_mode_app_ui.h"
#include "chromeos/components/demo_mode_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_demo_mode_app_resources.h"
#include "chromeos/grit/chromeos_demo_mode_app_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

DemoModeAppUI::DemoModeAppUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chromeos::kChromeUIDemoModeAppHost);

  // Add required resources.
  for (size_t i = 0; i < kChromeosDemoModeAppResourcesSize; ++i) {
    html_source->AddResourcePath(kChromeosDemoModeAppResources[i].path,
                                 kChromeosDemoModeAppResources[i].id);
  }

  html_source->SetDefaultResource(
      IDR_CHROMEOS_DEMO_MODE_APP_DEMO_MODE_APP_HTML);

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, html_source);
}

DemoModeAppUI::~DemoModeAppUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(DemoModeAppUI)

}  // namespace chromeos
