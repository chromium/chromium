// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_TEST_CHROMEDRIVER_LOG_REPLAY_CHROME_REPLAY_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_LOG_REPLAY_CHROME_REPLAY_IMPL_H_

#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"

class DevToolsClient;
class DevToolsHttpClient;
class Status;

// Same as ChromeDesktopImpl except that it completely ignores the existence
// of the |process| passed into the constructor. This allows running Chrome
// with a connection to log-replay DevTools implementations without launching
// a Chrome process at all.
class ChromeReplayImpl : public ChromeDesktopImpl {
 public:
  ChromeReplayImpl(std::unique_ptr<DevToolsHttpClient> http_client,
                   std::unique_ptr<DevToolsClient> websocket_client,
                   std::vector<std::unique_ptr<DevToolsEventListener>>
                       devtools_event_listeners,
                   std::string page_load_strategy,
                   base::Process process,
                   const base::CommandLine& command,
                   base::ScopedTempDir* user_data_dir,
                   base::ScopedTempDir* extension_dir,
                   bool network_emulation_enabled);
  ~ChromeReplayImpl() override;

  // A no-op: all this does in DesktopChromeImpl is kill the Chrome process.
  Status QuitImpl() override;
};

#endif  // CHROME_TEST_CHROMEDRIVER_LOG_REPLAY_CHROME_REPLAY_IMPL_H_
