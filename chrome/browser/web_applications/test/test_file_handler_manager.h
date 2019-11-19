// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_FILE_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_FILE_HANDLER_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/web_applications/components/file_handler_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"
#include "url/gurl.h"

namespace web_app {

// A testing implementation of a file handler manager.
class TestFileHandlerManager : public FileHandlerManager {
 public:
  TestFileHandlerManager();
  ~TestFileHandlerManager() override;

  const std::vector<apps::FileHandlerInfo>* GetFileHandlers(
      const AppId& app_id) override;

  void InstallFileHandler(const AppId& app_id,
                          const GURL& handler,
                          std::vector<std::string> accepts);

 private:
  std::map<AppId, std::vector<apps::FileHandlerInfo>> file_handlers_;
  DISALLOW_COPY_AND_ASSIGN(TestFileHandlerManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_FILE_HANDLER_MANAGER_H_
