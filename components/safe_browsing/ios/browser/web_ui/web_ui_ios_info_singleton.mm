// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/safe_browsing/ios/browser/web_ui/web_ui_ios_info_singleton.h"

#import "base/functional/callback_helpers.h"
#import "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton_event_observer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/mojom/network_context.mojom.h"

namespace safe_browsing {
WebUIIOSInfoSingleton::WebUIIOSInfoSingleton() = default;

WebUIIOSInfoSingleton::~WebUIIOSInfoSingleton() = default;

// static
WebUIIOSInfoSingleton* WebUIIOSInfoSingleton::GetInstance() {
  static base::NoDestructor<WebUIIOSInfoSingleton> instance;
  return instance.get();
}

void WebUIIOSInfoSingleton::PostLogMessage(const base::Time timestamp,
                                           const std::string& message) {
  web::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&WebUIIOSInfoSingleton::NotifyLogMessageListeners,
                     timestamp, message));
}

mojo::Remote<network::mojom::CookieManager>
WebUIIOSInfoSingleton::GetCookieManager() {
  network::mojom::NetworkContext* network_context =
      GetApplicationContext()->GetSystemNetworkContext();
  return WebUIInfoSingleton::GetCookieManager(network_context);
}

/* static */ void WebUIIOSInfoSingleton::NotifyLogMessageListeners(
    const base::Time& timestamp,
    const std::string& message) {
  WebUIInfoSingleton* web_ui_info = GetInstance();
  web_ui_info->NotifyLogMessageListeners(timestamp, message);
}

}  // namespace safe_browsing
