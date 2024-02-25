// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_PROTOCOL_HANDLER_REGISTRATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_PROTOCOL_HANDLER_REGISTRATION_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

void RegisterProtocolHandlersWithOs(
    const webapps::AppId& app_id,
    const std::string& app_name,
    const base::FilePath profile_path,
    std::vector<apps::ProtocolHandlerInfo> protocol_handlers,
    ResultCallback callback);

void UnregisterProtocolHandlersWithOs(const webapps::AppId& app_id,
                                      const base::FilePath profile_path,
                                      ResultCallback callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_WEB_APP_PROTOCOL_HANDLER_REGISTRATION_H_
