// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_INSTALL_INFO_FROM_INSTALL_URL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_INSTALL_INFO_FROM_INSTALL_URL_COMMAND_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "components/webapps/browser/installable/installable_logging.h"

namespace web_app {

class SharedWebContentsLockDescription;
class SharedWebContentsLock;
class WebAppUrlLoader;
class WebAppDataRetriever;

enum class FetchInstallInfoResult {
  kAppInfoObtained,
  kWebContentsDestroyed,
  kInstallUrlInvalid,
  kManifestIdInvalid,
  kUrlLoadingFailure,
  kNoValidManifest,
  kWrongManifestId,
  kSystemShutdown,
  kFailure
};

std::ostream& operator<<(std::ostream& os, FetchInstallInfoResult result);

class FetchInstallInfoFromInstallUrlCommand
    : public WebAppCommandTemplate<SharedWebContentsLock> {
 public:
  FetchInstallInfoFromInstallUrlCommand(
      webapps::ManifestId manifest_id,
      GURL install_url,
      base::OnceCallback<void(std::unique_ptr<WebAppInstallInfo>)> callback);
  ~FetchInstallInfoFromInstallUrlCommand() override;
  FetchInstallInfoFromInstallUrlCommand(
      const FetchInstallInfoFromInstallUrlCommand&) = delete;
  FetchInstallInfoFromInstallUrlCommand& operator=(
      const FetchInstallInfoFromInstallUrlCommand&) = delete;

  // WebAppCommandTemplate<SharedWebContentsLock>:
  base::Value ToDebugValue() const override;

 protected:
  // WebAppCommandTemplate<SharedWebContentsLock>:
  void StartWithLock(std::unique_ptr<SharedWebContentsLock> lock) override;
  void OnShutdown() override;
  const LockDescription& lock_description() const override;

 private:
  bool IsWebContentsDestroyed();
  void OnWebAppUrlLoadedGetWebAppInstallInfo(WebAppUrlLoader::Result result);
  void OnGetWebAppInstallInfo(std::unique_ptr<WebAppInstallInfo> install_info);
  void OnManifestRetrieved(std::unique_ptr<WebAppInstallInfo> web_app_info,
                           blink::mojom::ManifestPtr opt_manifest,
                           const GURL& manifest_url,
                           bool valid_manifest_for_web_app,
                           webapps::InstallableStatusCode error_code);
  void OnIconsRetrieved(std::unique_ptr<WebAppInstallInfo> web_app_info,
                        IconsDownloadedResult result,
                        IconsMap icons_map,
                        DownloadedIconsHttpResults icons_http_results);
  void CompleteCommandAndSelfDestruct(
      FetchInstallInfoResult result,
      std::unique_ptr<WebAppInstallInfo> install_info);

  std::unique_ptr<SharedWebContentsLockDescription> lock_description_;
  std::unique_ptr<SharedWebContentsLock> lock_;

  webapps::ManifestId manifest_id_;
  GURL install_url_;
  base::OnceCallback<void(std::unique_ptr<WebAppInstallInfo>)>
      web_app_install_info_callback_;

  std::unique_ptr<WebAppUrlLoader> url_loader_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  InstallErrorLogEntry install_error_log_entry_;
  base::Value::Dict debug_log_;

  base::WeakPtrFactory<FetchInstallInfoFromInstallUrlCommand> weak_ptr_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_INSTALL_INFO_FROM_INSTALL_URL_COMMAND_H_
