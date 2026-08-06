// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_android_impl.h"

#include <utility>

#include "chrome/test/chromedriver/chrome/device_manager.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/status.h"

ChromeAndroidImpl::ChromeAndroidImpl(
    BrowserInfo browser_info,
    std::set<WebViewInfo::Type> window_types,
    std::unique_ptr<DevToolsClient> websocket_client,
    std::vector<std::unique_ptr<DevToolsEventListener>>
        devtools_event_listeners,
    std::optional<MobileDevice> mobile_device,
    std::string page_load_strategy,
    std::unique_ptr<Device> device,
    bool autoaccept_beforeunload,
    bool enable_extension_targets)
    : ChromeImpl(std::move(browser_info),
                 std::move(window_types),
                 std::move(websocket_client),
                 std::move(devtools_event_listeners),
                 std::move(mobile_device),
                 page_load_strategy,
                 autoaccept_beforeunload,
                 enable_extension_targets),
      device_(std::move(device)) {}

ChromeAndroidImpl::~ChromeAndroidImpl() = default;

Status ChromeAndroidImpl::GetAsDesktop(ChromeDesktopImpl** desktop) {
  return Status(kUnknownError, "operation is unsupported on Android");
}

std::string ChromeAndroidImpl::GetOperatingSystemName() {
  return "ANDROID";
}

// Window lookup and non-fullscreen state changes use ChromeImpl's Browser
// domain path now that Android implements the corresponding commands.
Status ChromeAndroidImpl::FullScreenWindow(const std::string& target_id) {
  return Status{kUnsupportedOperation,
                "Fullscreen mode is not supported on Android platform"};
}

bool ChromeAndroidImpl::HasTouchScreen() const {
  return true;
}

Status ChromeAndroidImpl::QuitImpl() {
  return device_->TearDown();
}
