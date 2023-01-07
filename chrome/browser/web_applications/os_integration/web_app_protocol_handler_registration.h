// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_PROTOCOL_HANDLER_REGISTRATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_PROTOCOL_HANDLER_REGISTRATION_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"

class Profile;

namespace web_app {

void RegisterProtocolHandlersWithOs(
    const AppId& app_id,
    const std::string& app_name,
    Profile* profile,
    std::vector<apps::ProtocolHandlerInfo> protocol_handlers,
    ResultCallback callback);

void UnregisterProtocolHandlersWithOs(const AppId& app_id,
                                      Profile* profile,
                                      ResultCallback callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_PROTOCOL_HANDLER_REGISTRATION_H_
