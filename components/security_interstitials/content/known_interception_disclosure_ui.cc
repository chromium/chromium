// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/known_interception_disclosure_ui.h"

#include "build/build_config.h"
#include "components/grit/components_resources.h"
#include "components/security_interstitials/content/urls.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_interstitials {

KnownInterceptionDisclosureUIConfig::KnownInterceptionDisclosureUIConfig()
    : DefaultWebUIConfig(
          content::kChromeUIScheme,
          security_interstitials::kChromeUIConnectionMonitoringDetectedHost) {}

KnownInterceptionDisclosureUI::KnownInterceptionDisclosureUI(
    content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          kChromeUIConnectionMonitoringDetectedHost);

  html_source->AddLocalizedString("title", IDS_KNOWN_INTERCEPTION_TITLE);
  html_source->AddLocalizedString("pageHeader", IDS_KNOWN_INTERCEPTION_HEADER);
  html_source->AddLocalizedString("pageBody1", IDS_KNOWN_INTERCEPTION_BODY1);
  html_source->AddLocalizedString("pageBody2", IDS_KNOWN_INTERCEPTION_BODY2);

  html_source->AddResourcePath("interstitial_core.css",
                               IDR_SECURITY_INTERSTITIAL_CORE_CSS);
  html_source->AddResourcePath("interstitial_common.css",
                               IDR_SECURITY_INTERSTITIAL_COMMON_CSS);
  html_source->AddResourcePath("monitoring_disclosure.css",
                               IDR_KNOWN_INTERCEPTION_CSS);
  html_source->AddResourcePath("images/1x/triangle_red.png",
                               IDR_KNOWN_INTERCEPTION_ICON_1X_PNG);
  html_source->AddResourcePath("images/2x/triangle_red.png",
                               IDR_KNOWN_INTERCEPTION_ICON_2X_PNG);
  html_source->SetDefaultResource(IDR_KNOWN_INTERCEPTION_HTML);
}

KnownInterceptionDisclosureUI::~KnownInterceptionDisclosureUI() = default;

}  // namespace security_interstitials
