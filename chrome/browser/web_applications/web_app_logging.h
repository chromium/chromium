// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LOGGING_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LOGGING_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"

namespace webapps {
enum class WebAppUrlLoaderResult;
}

namespace web_app {

// Helper class to accumulate structured error information during a web app
// installation process. This is only enabled if the
// |kRecordWebAppDebugInfo| feature flag is enabled. The logs are sent to the
// WebAppInstallManager and are exposed in chrome://web-app-internals.
//
// An InstallErrorLogEntry is typically created at the beginning of an install
// command and is passed around. Various stages of the installation process can
// log errors to it. At the end of the command, if any errors were logged, the
// entire log entry is sent to the install manager. This is done by the command
// calling `WebAppCommandManager::LogToInstallManager`, which then calls
// `WebAppInstallManager::TakeCommandErrorLog`.
class InstallErrorLogEntry {
 public:
  explicit InstallErrorLogEntry(bool background_installation,
                                webapps::WebappInstallSource install_surface);
  ~InstallErrorLogEntry();

  // The InstallWebAppTask determines this after construction, so a setter is
  // required.
  void set_background_installation(bool background_installation) {
    background_installation_ = background_installation;
  }

  bool HasErrorDict() const { return !!error_dict_; }

  // Collects install errors (unbounded) if the |kRecordWebAppDebugInfo|
  // flag is enabled to be used by: chrome://web-app-internals
  base::Value::Dict TakeErrorDict();

  void LogUrlLoaderError(const char* stage,
                         const std::string& url,
                         webapps::WebAppUrlLoaderResult result);
  void LogExpectedAppIdError(const char* stage,
                             const std::string& url,
                             const webapps::AppId& app_id,
                             const webapps::AppId& expected_app_id);
  void LogDownloadedIconsErrors(
      const WebAppInstallInfo& web_app_info,
      IconsDownloadedResult icons_downloaded_result,
      const IconsMap& icons_map,
      const DownloadedIconsHttpResults& icons_http_results);

 private:
  void LogHeaderIfLogEmpty(const std::string& url);

  void LogErrorObject(const char* stage,
                      const std::string& url,
                      base::Value::Dict object);

  std::unique_ptr<base::Value::Dict> error_dict_;
  bool background_installation_;
  webapps::WebappInstallSource install_surface_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_LOGGING_H_
