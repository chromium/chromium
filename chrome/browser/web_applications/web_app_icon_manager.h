// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"

class Profile;
struct WebApplicationIconInfo;

namespace web_app {

class FileUtilsWrapper;
class WebAppRegistrar;

// Exclusively used from the UI thread.
class WebAppIconManager : public AppIconManager {
 public:
  WebAppIconManager(Profile* profile,
                    WebAppRegistrar& registrar,
                    std::unique_ptr<FileUtilsWrapper> utils);
  ~WebAppIconManager() override;

  // Writes all data (icons) for an app.
  using WriteDataCallback = base::OnceCallback<void(bool success)>;
  void WriteData(AppId app_id,
                 std::vector<WebApplicationIconInfo> icon_infos,
                 WriteDataCallback callback);
  void DeleteData(AppId app_id, WriteDataCallback callback);

  // AppIconManager:
  bool ReadIcon(const AppId& app_id,
                int icon_size_in_px,
                ReadIconCallback callback) override;
  bool ReadSmallestIcon(const AppId& app_id,
                        int icon_size_in_px,
                        ReadIconCallback callback) override;

 private:
  void ReadIconInternal(const AppId& app_id,
                        int icon_size_in_px,
                        ReadIconCallback callback);

  const WebAppRegistrar& registrar_;
  base::FilePath web_apps_directory_;
  std::unique_ptr<FileUtilsWrapper> utils_;

  DISALLOW_COPY_AND_ASSIGN(WebAppIconManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ICON_MANAGER_H_
