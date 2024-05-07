// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/test/chromedriver/log_replay/chrome_replay_impl.h"

#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

ChromeReplayImpl::ChromeReplayImpl(
    BrowserInfo browser_info,
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
    bool autoaccept_beforeunload)
    : ChromeDesktopImpl(std::move(browser_info),
                        std::move(window_types),
                        std::move(websocket_client),
                        std::move(devtools_event_listeners),
                        std::move(mobile_device),
                        page_load_strategy,
                        std::move(process),
                        command,
                        user_data_dir,
                        extension_dir,
                        network_emulation_enabled,
                        autoaccept_beforeunload) {}

ChromeReplayImpl::~ChromeReplayImpl() = default;

Status ChromeReplayImpl::QuitImpl() {
  return Status(kOk);
}
