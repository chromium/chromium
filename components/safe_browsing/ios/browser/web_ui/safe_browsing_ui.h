// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_IOS_BROWSER_WEB_UI_SAFE_BROWSING_UI_H_
#define COMPONENTS_SAFE_BROWSING_IOS_BROWSER_WEB_UI_SAFE_BROWSING_UI_H_

#import <sstream>

#import "components/safe_browsing/core/browser/web_ui/cr_safe_browsing_log.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"

namespace safe_browsing {

// The WebUI for chrome://safe-browsing
class SafeBrowsingUI : public web::WebUIIOSController {
 public:
  SafeBrowsingUI(web::WebUIIOS* web_ui, const std::string& host);
  SafeBrowsingUI(const SafeBrowsingUI&) = delete;
  SafeBrowsingUI& operator=(const SafeBrowsingUI&) = delete;
  ~SafeBrowsingUI() override;
};

class CrSBIOSLogMessage : public CrSBLogMessage {
 public:
  CrSBIOSLogMessage();

  ~CrSBIOSLogMessage() override;
};

#define CRSBLOG                                            \
  (!::safe_browsing::WebUIIOSInfoSingleton::HasListener()) \
      ? static_cast<void>(0)                               \
      : ::safe_browsing::CrSBLogVoidify() &                \
            ::safe_browsing::CrSBIOSLogMessage().stream()

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_IOS_BROWSER_WEB_UI_SAFE_BROWSING_UI_H_
