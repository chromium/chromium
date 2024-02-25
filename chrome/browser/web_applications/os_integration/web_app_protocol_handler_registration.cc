// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_registration.h"

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

// This block defines stub implementations of OS specific methods for
// FileHandling. Currently, Windows and MacOSX have their own
// implementations.
#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC)
// Registers a protocol handler for the web app with the OS.
void RegisterProtocolHandlersWithOs(
    const webapps::AppId& app_id,
    const std::string& app_name,
    const base::FilePath profile_path,
    std::vector<apps::ProtocolHandlerInfo> protocol_handlers,
    ResultCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), Result::kOk));
}

// Unregisters protocol handlers for a web app with the OS.
void UnregisterProtocolHandlersWithOs(const webapps::AppId& app_id,
                                      const base::FilePath profile_path,
                                      ResultCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), Result::kOk));
}
#endif

}  // namespace web_app
