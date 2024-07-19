// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_INSTALL_INFO_FROM_INSTALL_URL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_INSTALL_INFO_FROM_INSTALL_URL_COMMAND_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "components/webapps/browser/installable/installable_logging.h"

namespace webapps {
class WebAppUrlLoader;
}

namespace web_app {

class WebAppDataRetriever;

enum class FetchInstallInfoResult {
  kAppInfoObtained,
  kWebContentsDestroyed,
  kUrlLoadingFailure,
  kNoValidManifest,
  kWrongManifestId,
  kFailure
};

std::ostream& operator<<(std::ostream& os, FetchInstallInfoResult result);

class FetchInstallInfoFromInstallUrlCommand
    : public WebAppCommand<SharedWebContentsLock,
                           std::unique_ptr<WebAppInstallInfo>> {
 public:
  FetchInstallInfoFromInstallUrlCommand(
      webapps::ManifestId manifest_id,
      GURL install_url,
      std::optional<webapps::ManifestId> parent_manifest_id,
      base::OnceCallback<void(std::unique_ptr<WebAppInstallInfo>)> callback);
  ~FetchInstallInfoFromInstallUrlCommand() override;
  FetchInstallInfoFromInstallUrlCommand(
      const FetchInstallInfoFromInstallUrlCommand&) = delete;
  FetchInstallInfoFromInstallUrlCommand& operator=(
      const FetchInstallInfoFromInstallUrlCommand&) = delete;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<SharedWebContentsLock> lock) override;

 private:
  bool IsWebContentsDestroyed();
  void OnWebAppUrlLoadedGetWebAppInstallInfo(
      webapps::WebAppUrlLoaderResult result);
  void OnGetWebAppInstallInfo(std::unique_ptr<WebAppInstallInfo> install_info);
  void OnManifestRetrieved(std::unique_ptr<WebAppInstallInfo> web_app_info,
                           blink::mojom::ManifestPtr opt_manifest,
                           bool valid_manifest_for_web_app,
                           webapps::InstallableStatusCode error_code);
  void OnIconsRetrieved(std::unique_ptr<WebAppInstallInfo> web_app_info,
                        IconsDownloadedResult result,
                        IconsMap icons_map,
                        DownloadedIconsHttpResults icons_http_results);
  void CompleteCommandAndSelfDestruct(
      FetchInstallInfoResult result,
      std::unique_ptr<WebAppInstallInfo> install_info);

  std::unique_ptr<SharedWebContentsLock> lock_;

  const webapps::ManifestId manifest_id_;
  const GURL install_url_;
  const std::optional<webapps::ManifestId> parent_manifest_id_;

  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  InstallErrorLogEntry install_error_log_entry_;

  base::WeakPtrFactory<FetchInstallInfoFromInstallUrlCommand> weak_ptr_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_INSTALL_INFO_FROM_INSTALL_URL_COMMAND_H_
