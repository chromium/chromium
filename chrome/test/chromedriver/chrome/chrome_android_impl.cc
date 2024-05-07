// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_android_impl.h"

#include <utility>

#include "base/strings/string_split.h"
#include "chrome/test/chromedriver/chrome/device_manager.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"

ChromeAndroidImpl::ChromeAndroidImpl(
    BrowserInfo browser_info,
    std::set<WebViewInfo::Type> window_types,
    std::unique_ptr<DevToolsClient> websocket_client,
    std::vector<std::unique_ptr<DevToolsEventListener>>
        devtools_event_listeners,
    std::optional<MobileDevice> mobile_device,
    std::string page_load_strategy,
    std::unique_ptr<Device> device,
    bool autoaccept_beforeunload)
    : ChromeImpl(std::move(browser_info),
                 std::move(window_types),
                 std::move(websocket_client),
                 std::move(devtools_event_listeners),
                 std::move(mobile_device),
                 page_load_strategy,
                 autoaccept_beforeunload),
      device_(std::move(device)) {}

ChromeAndroidImpl::~ChromeAndroidImpl() = default;

Status ChromeAndroidImpl::GetAsDesktop(ChromeDesktopImpl** desktop) {
  return Status(kUnknownError, "operation is unsupported on Android");
}

std::string ChromeAndroidImpl::GetOperatingSystemName() {
  return "ANDROID";
}

Status ChromeAndroidImpl::GetWindow(const std::string& target_id,
                                    internal::Window& window) {
  WebView* web_view = nullptr;
  Status status = GetWebViewById(target_id, &web_view);
  if (status.IsError())
    return status;

  std::unique_ptr<base::Value> result;
  std::string expression =
      "[window.screenX, window.screenY, window.outerWidth, window.outerHeight]";
  status = web_view->EvaluateScript(target_id, expression, false, &result);
  if (status.IsError())
    return status;

  window.left = static_cast<int>(result->GetList()[0].GetDouble());
  window.top = static_cast<int>(result->GetList()[1].GetDouble());
  window.width = static_cast<int>(result->GetList()[2].GetDouble());
  window.height = static_cast<int>(result->GetList()[3].GetDouble());
  // Android does not use Window.id or have window states
  window.id = 0;
  window.state = "";

  return status;
}

Status ChromeAndroidImpl::MaximizeWindow(const std::string& target_id) {
  return Status{kUnsupportedOperation,
                "Unable to maximize window on Android platform"};
}

Status ChromeAndroidImpl::MinimizeWindow(const std::string& target_id) {
  return Status{kUnsupportedOperation,
                "Unable to minimize window on Android platform"};
}

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

