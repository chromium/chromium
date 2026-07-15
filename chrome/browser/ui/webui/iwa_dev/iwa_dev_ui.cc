// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/iwa_dev/iwa_dev_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/iwa_dev_resources.h"
#include "chrome/grit/iwa_dev_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

bool IwaDevUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return content::AreIsolatedWebAppsEnabled(browser_context) &&
         base::FeatureList::IsEnabled(features::kIsolatedWebAppDevUi);
}

IwaDevUI::IwaDevUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIIwaDevHost);
  webui::SetupWebUIDataSource(source, kIwaDevResources,
                              IDR_IWA_DEV_IWA_DEV_HTML);
  source->AddBoolean("isIwaDevModeEnabled",
                     web_app::IsIwaDevModeEnabled(profile));
}

IwaDevUI::~IwaDevUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(IwaDevUI)
