// Copyright (c) 2013 The Chromium Authors. All rights reserved.
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
    std::unique_ptr<DevToolsHttpClient> http_client,
    std::unique_ptr<DevToolsClient> websocket_client,
    std::vector<std::unique_ptr<DevToolsEventListener>>
        devtools_event_listeners,
    std::string page_load_strategy,
    std::unique_ptr<Device> device)
    : ChromeImpl(std::move(http_client),
                 std::move(websocket_client),
                 std::move(devtools_event_listeners),
                 page_load_strategy),
      device_(std::move(device)) {}

ChromeAndroidImpl::~ChromeAndroidImpl() {}

Status ChromeAndroidImpl::GetAsDesktop(ChromeDesktopImpl** desktop) {
  return Status(kUnknownError, "operation is unsupported on Android");
}

std::string ChromeAndroidImpl::GetOperatingSystemName() {
  return "ANDROID";
}

Status ChromeAndroidImpl::GetWindow(const std::string& target_id,
                                    Window* window) {
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

  window->left = static_cast<int>(result->GetList()[0].GetDouble());
  window->top = static_cast<int>(result->GetList()[1].GetDouble());
  window->width = static_cast<int>(result->GetList()[2].GetDouble());
  window->height = static_cast<int>(result->GetList()[3].GetDouble());
  // Android does not use Window.id or have window states
  window->id = 0;
  window->state = "";

  return status;
}

bool ChromeAndroidImpl::HasTouchScreen() const {
  return true;
}

Status ChromeAndroidImpl::QuitImpl() {
  return device_->TearDown();
}

