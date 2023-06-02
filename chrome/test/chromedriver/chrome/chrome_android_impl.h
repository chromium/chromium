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
class DevToolsClient;

class ChromeAndroidImpl : public ChromeImpl {
 public:
  ChromeAndroidImpl(BrowserInfo browser_info,
                    std::set<WebViewInfo::Type> window_types,
                    std::unique_ptr<DevToolsClient> websocket_client,
                    std::vector<std::unique_ptr<DevToolsEventListener>>
                        devtools_event_listeners,
                    absl::optional<MobileDevice> mobile_device,
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
