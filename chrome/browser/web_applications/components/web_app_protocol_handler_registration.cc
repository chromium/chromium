// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_protocol_handler_registration.h"

#include "build/build_config.h"
#include "chrome/browser/web_applications/components/web_app_id.h"

namespace web_app {

#if !defined(OS_WIN)
// Registers a protocol handler for the web app with the OS.
//
// TODO(crbug.com/1174805): Add a callback as part of the protocol handling
// registration flow.
void RegisterProtocolHandlersWithOs(
    const AppId& app_id,
    const std::string& app_name,
    Profile* profile,
    std::vector<apps::ProtocolHandlerInfo> protocol_handlers) {}

// Unregisters a protocol handler for the web app with the OS.
//
// TODO(crbug.com/1174805): Add a callback as part of the protocol handling
// unregistration flow.
void UnregisterProtocolHandlersWithOs(
    const AppId& app_id,
    Profile* profile,
    std::vector<apps::ProtocolHandlerInfo> protocol_handlers) {}
#endif

}  // namespace web_app
