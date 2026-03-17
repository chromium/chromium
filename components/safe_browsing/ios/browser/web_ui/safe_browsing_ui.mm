// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/safe_browsing/ios/browser/web_ui/safe_browsing_ui.h"

#import "components/grit/safe_browsing_resources.h"
#import "components/grit/safe_browsing_resources_map.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/web_ui_constants.h"
#import "components/safe_browsing/ios/browser/web_ui/chrome_ios_safe_browsing_local_state_delegate.h"
#import "components/safe_browsing/ios/browser/web_ui/safe_browsing_ios_ui_handler.h"
#import "components/safe_browsing/ios/browser/web_ui/web_ui_ios_info_singleton.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"

namespace {

web::WebUIIOSDataSource* CreateSafeBrowsingUIDataSource() {
  web::WebUIIOSDataSource* html_source =
      web::WebUIIOSDataSource::Create(safe_browsing::kChromeUISafeBrowsingHost);
  // Add required resources.
  html_source->AddResourcePaths(kSafeBrowsingResources);
  html_source->AddResourcePath("", IDR_SAFE_BROWSING_SAFE_BROWSING_HTML);
  return html_source;
}

}  // namespace

namespace safe_browsing {

SafeBrowsingUI::SafeBrowsingUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  // Register callback handler.
  // Handles messages from JavaScript to C++ via chrome.send().
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui);
  os_crypt_async::OSCryptAsync* os_crypt_async =
      GetApplicationContext()->GetOSCryptAsync();
  web_ui->AddMessageHandler(std::make_unique<SafeBrowsingIOSUIHandler>(
      profile, std::make_unique<ChromeIOSSafeBrowsingLocalStateDelegate>(),
      os_crypt_async));
  // Set up the chrome://safe-browsing source.
  web::WebUIIOSDataSource::Add(profile, CreateSafeBrowsingUIDataSource());
}

SafeBrowsingUI::~SafeBrowsingUI() = default;

CrSBIOSLogMessage::CrSBIOSLogMessage() = default;

CrSBIOSLogMessage::~CrSBIOSLogMessage() {
  CrSBLogMessage::LogStreamToInfoSingleton(
      WebUIIOSInfoSingleton::GetInstance());
}

}  // namespace safe_browsing
