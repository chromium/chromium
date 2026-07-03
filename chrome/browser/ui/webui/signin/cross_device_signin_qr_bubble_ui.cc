// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/cross_device_signin_qr_bubble_ui.h"

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/signin_resources.h"
#include "chrome/grit/signin_resources_map.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

CrossDeviceSigninQrBubbleUIConfig::CrossDeviceSigninQrBubbleUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUICrossDeviceSigninQrBubbleHost) {
  // NOLINT(modernize-use-equals-default): DefaultWebUIConfig requires
  // arguments.
}

bool CrossDeviceSigninQrBubbleUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(switches::kCrossDeviceSigninFromDesktop);
}

CrossDeviceSigninQrBubbleUI::CrossDeviceSigninQrBubbleUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui),
      chrome::kChromeUICrossDeviceSigninQrBubbleHost);

  webui::SetupWebUIDataSource(
      source, base::span(kSigninResources),
      IDR_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_CROSS_DEVICE_SIGNIN_QR_BUBBLE_HTML);
  source->AddLocalizedString("crossDeviceSigninTitle",
                             IDS_QR_CODE_BUBBLE_SIGNIN_ON_PHONE_TITLE);
}

CrossDeviceSigninQrBubbleUI::~CrossDeviceSigninQrBubbleUI() = default;
