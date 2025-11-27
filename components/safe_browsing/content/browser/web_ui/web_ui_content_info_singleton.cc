// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/web_ui/web_ui_content_info_singleton.h"

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton_event_observer.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace safe_browsing {
WebUIContentInfoSingleton::WebUIContentInfoSingleton() = default;

WebUIContentInfoSingleton::~WebUIContentInfoSingleton() = default;

// static
WebUIContentInfoSingleton* WebUIContentInfoSingleton::GetInstance() {
  CHECK(base::CommandLine::ForCurrentProcess()
            ->GetSwitchValueASCII("type")
            .empty())
      << "chrome://safe-browsing WebUI is only available in the browser "
         "process";
  static base::NoDestructor<WebUIContentInfoSingleton> instance;
  return instance.get();
}

void WebUIContentInfoSingleton::LogMessage(const std::string& message) {
  if (!HasListener()) {
    return;
  }

  base::Time timestamp = base::Time::Now();
  log_messages_.push_back(std::make_pair(timestamp, message));

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&WebUIContentInfoSingleton::NotifyLogMessageListeners,
                     timestamp, message));
}

/* static */ void WebUIContentInfoSingleton::NotifyLogMessageListeners(
    const base::Time& timestamp,
    const std::string& message) {
  WebUIInfoSingleton* web_ui_info = GetInstance();
  for (safe_browsing::WebUIInfoSingletonEventObserver* webui_listener :
       web_ui_info->webui_instances()) {
    webui_listener->NotifyLogMessageJsListener(timestamp, message);
  }
}

mojo::Remote<network::mojom::CookieManager>
WebUIContentInfoSingleton::GetCookieManager(
    content::BrowserContext* browser_context) {
  mojo::Remote<network::mojom::CookieManager> cookie_manager_remote;
  if (sb_service_) {
    sb_service_->GetNetworkContext(browser_context)
        ->GetCookieManager(cookie_manager_remote.BindNewPipeAndPassReceiver());
  }

  return cookie_manager_remote;
}

ReferrerChainProvider* WebUIContentInfoSingleton::GetReferrerChainProvider(
    content::BrowserContext* browser_context) {
  if (!sb_service_) {
    return nullptr;
  }

  return sb_service_->GetReferrerChainProviderFromBrowserContext(
      browser_context);
}

#if BUILDFLAG(IS_ANDROID)
internal::ReferringAppInfo WebUIContentInfoSingleton::GetReferringAppInfo(
    content::WebContents* web_contents) {
  return sb_service_ ? sb_service_->GetReferringAppInfo(web_contents)
                     : internal::ReferringAppInfo{};
}
#endif


}  // namespace safe_browsing
