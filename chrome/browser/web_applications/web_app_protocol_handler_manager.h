// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROTOCOL_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROTOCOL_HANDLER_MANAGER_H_

#include "chrome/browser/web_applications/components/protocol_handler_manager.h"

namespace web_app {

class WebAppProtocolHandlerManager : public ProtocolHandlerManager {
 public:
  explicit WebAppProtocolHandlerManager(Profile* profile);
  ~WebAppProtocolHandlerManager() override;

 protected:
  std::vector<apps::ProtocolHandlerInfo> GetAppProtocolHandlerInfos(
      const std::string& app_id) const override;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_PROTOCOL_HANDLER_MANAGER_H_
