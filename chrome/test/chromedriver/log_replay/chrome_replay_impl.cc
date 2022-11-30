// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/test/chromedriver/log_replay/chrome_replay_impl.h"

#include "chrome/test/chromedriver/chrome/device_metrics.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

ChromeReplayImpl::ChromeReplayImpl(
    std::unique_ptr<DevToolsHttpClient> http_client,
    std::unique_ptr<DevToolsClient> websocket_client,
    std::vector<std::unique_ptr<DevToolsEventListener>>
        devtools_event_listeners,
    std::unique_ptr<DeviceMetrics> device_metrics,
    SyncWebSocketFactory socket_factory,
    std::string page_load_strategy,
    base::Process process,
    const base::CommandLine& command,
    base::ScopedTempDir* user_data_dir,
    base::ScopedTempDir* extension_dir,
    bool network_emulation_enabled)
    : ChromeDesktopImpl(std::move(http_client),
                        std::move(websocket_client),
                        std::move(devtools_event_listeners),
                        std::move(device_metrics),
                        std::move(socket_factory),
                        page_load_strategy,
                        std::move(process),
                        command,
                        user_data_dir,
                        extension_dir,
                        network_emulation_enabled) {}

ChromeReplayImpl::~ChromeReplayImpl() {}

Status ChromeReplayImpl::QuitImpl() {
  return Status(kOk);
}
