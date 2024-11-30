// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_TARGET_UTILS_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_TARGET_UTILS_H_

#include <stddef.h>

#include <memory>

#include "chrome/test/chromedriver/chrome/status.h"

class DevToolsClient;
class Timeout;
class WebViewsInfo;

namespace target_utils {

Status GetTopLevelViewsInfo(DevToolsClient& devtools_websocket_client,
                            const Timeout* timeout,
                            WebViewsInfo& views_info);
Status WaitForTab(DevToolsClient& client, const Timeout& timeout);
Status AttachToPageOrTabTarget(DevToolsClient& browser_client,
                               const std::string& target_id,
                               const Timeout* timeout,
                               std::unique_ptr<DevToolsClient>& target_client,
                               bool is_tab);

}  // namespace target_utils

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_TARGET_UTILS_H_
