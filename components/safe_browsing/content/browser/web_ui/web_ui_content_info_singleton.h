// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_WEB_UI_CONTENT_INFO_SINGLETON_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_WEB_UI_CONTENT_INFO_SINGLETON_H_

#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/safe_browsing_service_interface.h"
#include "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace content {
class BrowserContext;
}

namespace safe_browsing {
class ReferrerChainProvider;
class SafeBrowsingServiceInterface;

class WebUIContentInfoSingleton : public WebUIInfoSingleton {
 public:
  WebUIContentInfoSingleton();
  ~WebUIContentInfoSingleton() override;

  static WebUIContentInfoSingleton* GetInstance();

  WebUIContentInfoSingleton(const WebUIContentInfoSingleton&) = delete;
  WebUIContentInfoSingleton& operator=(const WebUIContentInfoSingleton&) =
      delete;

  // WebUIInfoSingleton::
  void LogMessage(const std::string& message) override;

  // Notify listeners of changes to the log messages. Static to avoid this being
  // called after the destruction of the WebUIInfoSingleton
  static void NotifyLogMessageListeners(const base::Time& timestamp,
                                        const std::string& message);

  mojo::Remote<network::mojom::CookieManager> GetCookieManager(
      content::BrowserContext* browser_context);

  ReferrerChainProvider* GetReferrerChainProvider(
      content::BrowserContext* browser_context);

#if BUILDFLAG(IS_ANDROID)
  internal::ReferringAppInfo GetReferringAppInfo(
      content::WebContents* web_contents);
#endif

  void set_safe_browsing_service(SafeBrowsingServiceInterface* sb_service) {
    sb_service_ = sb_service;
  }

 private:
  // The Safe Browsing service.
  raw_ptr<SafeBrowsingServiceInterface> sb_service_ = nullptr;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_WEB_UI_CONTENT_INFO_SINGLETON_H_
