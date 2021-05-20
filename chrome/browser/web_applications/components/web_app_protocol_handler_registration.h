// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_PROTOCOL_HANDLER_REGISTRATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_PROTOCOL_HANDLER_REGISTRATION_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"

class Profile;

namespace web_app {
using AppId = std::string;
}

namespace web_app {

void RegisterProtocolHandlersWithOs(
    const AppId& app_id,
    const std::string& app_name,
    Profile* profile,
    std::vector<apps::ProtocolHandlerInfo> protocol_handlers,
    base::OnceCallback<void(bool)> callback);

void UnregisterProtocolHandlersWithOs(
    const AppId& app_id,
    Profile* profile,
    std::vector<apps::ProtocolHandlerInfo> protocol_handlers);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_WEB_APP_PROTOCOL_HANDLER_REGISTRATION_H_
