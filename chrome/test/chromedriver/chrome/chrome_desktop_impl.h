// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_DESKTOP_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_DESKTOP_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/process/process.h"
#include "chrome/test/chromedriver/chrome/chrome_impl.h"
#include "chrome/test/chromedriver/chrome/scoped_temp_dir_with_retry.h"

namespace base {
class TimeDelta;
}

class DevToolsClient;
class Status;
class WebView;

class ChromeDesktopImpl : public ChromeImpl {
 public:
  ChromeDesktopImpl(BrowserInfo browser_info,
                    std::set<WebViewInfo::Type> window_types,
                    std::unique_ptr<DevToolsClient> websocket_client,
                    std::vector<std::unique_ptr<DevToolsEventListener>>
                        devtools_event_listeners,
                    std::optional<MobileDevice> mobile_device,
                    std::string page_load_strategy,
                    base::Process process,
                    const base::CommandLine& command,
                    base::ScopedTempDir* user_data_dir,
                    base::ScopedTempDir* extension_dir,
                    bool network_emulation_enabled,
                    bool autoaccept_beforeunload);
  ~ChromeDesktopImpl() override;

  // Waits for a page with the given URL to appear and finish loading.
  // Returns an error if the timeout is exceeded.
  Status WaitForPageToLoad(const std::string& url,
                           const base::TimeDelta& timeout,
                           std::unique_ptr<WebView>* web_view,
                           bool w3c_compliant);

  // Overridden from Chrome:
  Status GetAsDesktop(ChromeDesktopImpl** desktop) override;
  std::string GetOperatingSystemName() override;

  // Overridden from ChromeImpl:
  bool IsMobileEmulationEnabled() const override;
  bool HasTouchScreen() const override;
  Status QuitImpl() override;

  const base::CommandLine& command() const;
  bool IsNetworkConnectionEnabled() const;

  int GetNetworkConnection() const;
  void SetNetworkConnection(int network_connection);

 private:

  base::Process process_;
  base::CommandLine command_;
  ScopedTempDirWithRetry user_data_dir_;
  ScopedTempDirWithRetry extension_dir_;
  bool network_connection_enabled_;
  int network_connection_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_DESKTOP_IMPL_H_
