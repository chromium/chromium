// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_registration.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace web_app {

#if !BUILDFLAG(IS_WIN)
// Registers a protocol handler for the web app with the OS.
void RegisterProtocolHandlersWithOs(
    const AppId& app_id,
    const std::string& app_name,
    Profile* profile,
    std::vector<apps::ProtocolHandlerInfo> protocol_handlers,
    ResultCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), Result::kOk));
}

// Unregisters protocol handlers for a web app with the OS.
void UnregisterProtocolHandlersWithOs(const AppId& app_id,
                                      Profile* profile,
                                      ResultCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), Result::kOk));
}
#endif

}  // namespace web_app
