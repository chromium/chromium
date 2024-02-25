// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_SYNC_TEST_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_SYNC_TEST_UTILS_H_

#include <memory>
#include <vector>

#include "components/webapps/common/web_app_id.h"

namespace web_app {

class WebAppSyncBridge;
class WebApp;

namespace sync_bridge_test_utils {

void AddApps(WebAppSyncBridge& sync_bridge,
             const std::vector<std::unique_ptr<WebApp>>& apps_server_state);

void UpdateApps(WebAppSyncBridge& sync_bridge,
                const std::vector<std::unique_ptr<WebApp>>& apps_server_state);

void DeleteApps(WebAppSyncBridge& sync_bridge,
                const std::vector<webapps::AppId>& app_ids_to_delete);

}  // namespace sync_bridge_test_utils

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_SYNC_TEST_UTILS_H_
