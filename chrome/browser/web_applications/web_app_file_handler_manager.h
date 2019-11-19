// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILE_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILE_HANDLER_MANAGER_H_

#include <vector>

#include "chrome/browser/web_applications/components/file_handler_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"

class Profile;

namespace web_app {

class WebAppFileHandlerManager : public FileHandlerManager {
 public:
  explicit WebAppFileHandlerManager(Profile* profile);
  ~WebAppFileHandlerManager() override;

  const std::vector<apps::FileHandlerInfo>* GetFileHandlers(
      const AppId& app_id) override;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_FILE_HANDLER_MANAGER_H_
