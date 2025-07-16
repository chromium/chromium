// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/watermark/watermark_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/enterprise/watermark/watermark_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/watermark_resources.h"
#include "chrome/grit/watermark_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

WatermarkUIConfig::WatermarkUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIWatermarkHost) {}

bool WatermarkUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      enterprise_watermark::kEnableWatermarkTestPage);
}

WatermarkUI::WatermarkUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://watermark source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIWatermarkHost);

  // Add required resources.
  webui::SetupWebUIDataSource(source, kWatermarkResources,
                              IDR_WATERMARK_WATERMARK_HTML);

  // Pass a simple message to the frontend.
  source->AddString("message", "This is a watermark page!");
}

WatermarkUI::~WatermarkUI() = default;
