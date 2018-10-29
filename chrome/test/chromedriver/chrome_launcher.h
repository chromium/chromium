// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_LAUNCHER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_LAUNCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/process/kill.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"

class DevToolsEventListener;

namespace base {
class DictionaryValue;
class FilePath;
enum TerminationStatus;
}

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

class Chrome;
class DeviceManager;
class Status;

Status LaunchChrome(network::mojom::URLLoaderFactory* factory,
                    const SyncWebSocketFactory& socket_factory,
                    DeviceManager* device_manager,
                    const Capabilities& capabilities,
                    std::vector<std::unique_ptr<DevToolsEventListener>>
                        devtools_event_listeners,
                    std::unique_ptr<Chrome>* chrome,
                    bool w3c_compliant);

namespace internal {
Status ProcessExtensions(const std::vector<std::string>& extensions,
                         const base::FilePath& temp_dir,
                         bool include_automation_extension,
                         Switches* switches,
                         std::vector<std::string>* bg_pages);
Status PrepareUserDataDir(
    const base::FilePath& user_data_dir,
    const base::DictionaryValue* custom_prefs,
    const base::DictionaryValue* custom_local_state);
Status ParseDevToolsActivePortFile(const base::FilePath& user_data_dir,
                                   int* port);
Status RemoveOldDevToolsActivePortFile(const base::FilePath& user_data_dir);
std::string GetTerminationReason(base::TerminationStatus status);
}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_LAUNCHER_H_
