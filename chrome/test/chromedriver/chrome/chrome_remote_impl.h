// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_REMOTE_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_REMOTE_IMPL_H_

#include <memory>
#include <string>

#include "chrome/test/chromedriver/chrome/chrome_impl.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"

class DevToolsClient;
class DevToolsHttpClient;
struct DeviceMetrics;

class ChromeRemoteImpl : public ChromeImpl {
 public:
  ChromeRemoteImpl(std::unique_ptr<DevToolsHttpClient> http_client,
                   std::unique_ptr<DevToolsClient> websocket_client,
                   std::vector<std::unique_ptr<DevToolsEventListener>>
                       devtools_event_listeners,
                   std::unique_ptr<DeviceMetrics> device_metrics,
                   SyncWebSocketFactory socket_factory,
                   std::string page_load_strategy);
  ~ChromeRemoteImpl() override;

  // Overridden from Chrome.
  Status GetAsDesktop(ChromeDesktopImpl** desktop) override;
  std::string GetOperatingSystemName() override;

  // Overridden from ChromeImpl.
  Status QuitImpl() override;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_REMOTE_IMPL_H_
