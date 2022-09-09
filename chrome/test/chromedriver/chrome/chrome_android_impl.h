// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_ANDROID_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_ANDROID_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/test/chromedriver/chrome/chrome_impl.h"

class Device;
struct DeviceMetrics;
class DevToolsClient;
class DevToolsHttpClient;

class ChromeAndroidImpl : public ChromeImpl {
 public:
  ChromeAndroidImpl(std::unique_ptr<DevToolsHttpClient> http_client,
                    std::unique_ptr<DevToolsClient> websocket_client,
                    std::vector<std::unique_ptr<DevToolsEventListener>>
                        devtools_event_listeners,
                    std::unique_ptr<DeviceMetrics> device_metrics,
                    SyncWebSocketFactory socket_factory,
                    std::string page_load_strategy,
                    std::unique_ptr<Device> device);
  ~ChromeAndroidImpl() override;

  // Overridden from Chrome:
  Status GetAsDesktop(ChromeDesktopImpl** desktop) override;
  std::string GetOperatingSystemName() override;

  // Overridden from ChromeImpl:
  bool HasTouchScreen() const override;
  Status QuitImpl() override;

 protected:
  Status GetWindow(const std::string& target_id, Window* window) override;

 private:
  std::unique_ptr<Device> device_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_ANDROID_IMPL_H_
