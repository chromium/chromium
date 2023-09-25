// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_FILE_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_FILE_HANDLER_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

class FakeWebAppFileHandlerManager : public WebAppFileHandlerManager {
 public:
  explicit FakeWebAppFileHandlerManager(Profile* profile);
  FakeWebAppFileHandlerManager(const FakeWebAppFileHandlerManager&) = delete;
  FakeWebAppFileHandlerManager& operator=(const FakeWebAppFileHandlerManager&) =
      delete;
  ~FakeWebAppFileHandlerManager() override;

  const apps::FileHandlers* GetAllFileHandlers(
      const webapps::AppId& app_id) const override;

  bool IsDisabledForTesting() override;

  using AcceptMap = std::map<std::string, base::flat_set<std::string>>;
  // Installs a file handler for |app_id| with the action url |handler|,
  // accepting all mimetypes and extensions in |accepts|.
  // Note: If an item in accepts starts with a '.' it is considered an
  // extension, otherwise it is a mime.
  // Note: |enable| indicates whether file handlers for |app_id| should be
  // enabled, not whether this specific file handler should be enabled. If any
  // file handler is enabled, all of them will be.
  void InstallFileHandler(
      const webapps::AppId& app_id,
      const GURL& handler,
      const AcceptMap& accept,
      absl::optional<apps::FileHandler::LaunchType> launch_type,
      bool enable = true);

 private:
  std::map<webapps::AppId, apps::FileHandlers> file_handlers_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_FILE_HANDLER_MANAGER_H_
