// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_FILE_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_FILE_HANDLER_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "chrome/browser/web_applications/components/file_handler_manager.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

// A testing implementation of a file handler manager.
class TestFileHandlerManager : public FileHandlerManager {
 public:
  explicit TestFileHandlerManager(Profile* profile);
  TestFileHandlerManager(const TestFileHandlerManager&) = delete;
  TestFileHandlerManager& operator=(const TestFileHandlerManager&) = delete;
  ~TestFileHandlerManager() override;

  const apps::FileHandlers* GetAllFileHandlers(const AppId& app_id) override;

  using AcceptMap = std::map<std::string, base::flat_set<std::string>>;
  // Installs a file handler for |app_id| with the action url |handler|,
  // accepting all mimetypes and extensions in |accepts|.
  // Note: If an item in accepts starts with a '.' it is considered an
  // extension, otherwise it is a mime.
  // Note: |enable| indicates whether file handlers for |app_id| should be
  // enabled, not whether this specific file handler should be enabled. If any
  // file handler is enabled, all of them will be.
  void InstallFileHandler(const AppId& app_id,
                          const GURL& handler,
                          const AcceptMap& accept,
                          bool enable = true);

 private:
  std::map<AppId, apps::FileHandlers> file_handlers_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_FILE_HANDLER_MANAGER_H_
