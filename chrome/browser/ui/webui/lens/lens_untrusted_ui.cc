// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/lens/lens_untrusted_ui.h"

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/lens_untrusted_resources.h"
#include "chrome/grit/lens_untrusted_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace lens {

const char kScreenshotPath[] = "screenshot.png";

bool ShouldLoadScreenshot(const std::string& path) {
  return path == kScreenshotPath;
}

LensUntrustedUI::LensUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  // Set up the chrome-untrusted://lens source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUILensUntrustedURL);
  Profile* profile = Profile::FromWebUI(web_ui);
  if (!profile) {
    return;
  }

  // Add required resources.
  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kLensUntrustedResources, kLensUntrustedResourcesSize),
      IDR_LENS_UNTRUSTED_REGION_SEARCH_UNTRUSTED_HTML);
  html_source->AddFrameAncestor(GURL(chrome::kChromeUILensURL));
  // Allows chrome:://lens to load this page in an iframe.
  html_source->OverrideCrossOriginOpenerPolicy("same-origin");
  html_source->OverrideCrossOriginEmbedderPolicy("require-corp");
  lens::RegionSearchCapturedData* region_search_data =
      static_cast<lens::RegionSearchCapturedData*>(
          profile->GetUserData(lens::RegionSearchCapturedData::kDataKey));
  image_ = region_search_data->image;

  // Set request filter for loading the screenshot on the page.
  html_source->SetRequestFilter(
      base::BindRepeating(&ShouldLoadScreenshot),
      base::BindRepeating(&LensUntrustedUI::StartLoadScreenshot,
                          weak_factory_.GetWeakPtr()));
}

void LensUntrustedUI::StartLoadScreenshot(
    const std::string& resource_path,
    content::WebUIDataSource::GotDataCallback got_data_callback) {
  if (!image_.IsEmpty()) {
    std::move(got_data_callback).Run(image_.As1xPNGBytes());
  } else {
    std::move(got_data_callback).Run(nullptr);
  }
}

LensUntrustedUI::~LensUntrustedUI() = default;

}  // namespace lens
