// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_REMOTE_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_REMOTE_IMPL_H_

#include <memory>
#include <string>

#include "chrome/test/chromedriver/chrome/chrome_impl.h"
#include "chrome/test/chromedriver/chrome/mobile_device.h"

class DevToolsClient;

class ChromeRemoteImpl : public ChromeImpl {
 public:
  ChromeRemoteImpl(BrowserInfo browser_info,
                   std::set<WebViewInfo::Type> window_types,
                   std::unique_ptr<DevToolsClient> websocket_client,
                   std::vector<std::unique_ptr<DevToolsEventListener>>
                       devtools_event_listeners,
                   std::optional<MobileDevice> mobile_device,
                   std::string page_load_strategy,
                   bool autoaccept_beforeunload);
  ~ChromeRemoteImpl() override;

  // Overridden from Chrome.
  Status GetAsDesktop(ChromeDesktopImpl** desktop) override;
  std::string GetOperatingSystemName() override;

  // Overridden from ChromeImpl.
  Status QuitImpl() override;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_REMOTE_IMPL_H_
