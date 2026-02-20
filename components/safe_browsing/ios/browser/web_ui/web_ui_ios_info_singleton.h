// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_IOS_BROWSER_WEB_UI_WEB_UI_IOS_INFO_SINGLETON_H_
#define COMPONENTS_SAFE_BROWSING_IOS_BROWSER_WEB_UI_WEB_UI_IOS_INFO_SINGLETON_H_

#import "components/safe_browsing/buildflags.h"
#import "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton.h"

namespace safe_browsing {

class WebUIIOSInfoSingleton : public WebUIInfoSingleton {
 public:
  WebUIIOSInfoSingleton();
  ~WebUIIOSInfoSingleton() override;

  static WebUIIOSInfoSingleton* GetInstance();

  WebUIIOSInfoSingleton(const WebUIIOSInfoSingleton&) = delete;
  WebUIIOSInfoSingleton& operator=(const WebUIIOSInfoSingleton&) = delete;

  // WebUIIOSInfoSingleton::
  void PostLogMessage(base::Time timestamp,
                      const std::string& message) override;

  mojo::Remote<network::mojom::CookieManager> GetCookieManager();

  // Notify listeners of changes to the log messages. Static to avoid this being
  // called after the destruction of the WebUIIOSInfoSingleton
  static void NotifyLogMessageListeners(const base::Time& timestamp,
                                        const std::string& message);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_IOS_BROWSER_WEB_UI_WEB_UI_IOS_INFO_SINGLETON_H_
