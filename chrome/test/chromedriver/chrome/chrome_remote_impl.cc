// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_remote_impl.h"

#include <utility>

#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

ChromeRemoteImpl::ChromeRemoteImpl(
    BrowserInfo browser_info,
    std::set<WebViewInfo::Type> window_types,
    std::unique_ptr<DevToolsClient> websocket_client,
    std::vector<std::unique_ptr<DevToolsEventListener>>
        devtools_event_listeners,
    std::optional<MobileDevice> mobile_device,
    std::string page_load_strategy,
    bool autoaccept_beforeunload)
    : ChromeImpl(std::move(browser_info),
                 std::move(window_types),
                 std::move(websocket_client),
                 std::move(devtools_event_listeners),
                 std::move(mobile_device),
                 page_load_strategy,
                 autoaccept_beforeunload) {}

ChromeRemoteImpl::~ChromeRemoteImpl() = default;

Status ChromeRemoteImpl::GetAsDesktop(ChromeDesktopImpl** desktop) {
  return Status(kUnknownError,
                "operation is unsupported with remote debugging");
}

std::string ChromeRemoteImpl::GetOperatingSystemName() {
 return std::string();
}

Status ChromeRemoteImpl::QuitImpl() {
  return Status(kOk);
}

