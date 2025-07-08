// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_INSTALL_INFO_FROM_INSTALL_URL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_INSTALL_INFO_FROM_INSTALL_URL_COMMAND_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
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

// The result of a `FetchInstallInfoFromInstallUrlCommand`.
enum class FetchInstallInfoResult {
  // Successfully fetched the `WebAppInstallInfo`.
  kAppInfoObtained,
  // The web contents was destroyed before the command could complete.
  kWebContentsDestroyed,
  // The given `install_url` failed to load.
  kUrlLoadingFailure,
  // The site did not have a valid web app manifest.
  kNoValidManifest,
  // The manifest ID of the fetched manifest did not match the expected ID.
  kWrongManifestId,
  // A generic failure occurred.
  kFailure,
};

std::ostream& operator<<(std::ostream& os, FetchInstallInfoResult result);

// Fetches the WebAppInstallInfo for a given install URL. This is used for
// installing sub-apps, where the manifest ID and parent manifest ID are known,
// and a full install process is not necessary.
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

  // The command divides and goes into 2 paths here:
  // 1. If an opt_manifest is found and is valid, it uses that information to
  // create a `WebAppInstallInfo` instance, and carries over any fields from the
  // `WebAppInstallInfo` instance created from the page metadata if needed.
  // 2. Else, it uses the `WebAppInstallInfo` obtained from the page metadata to
  // retrieve icons and progress.
  void OnManifestRetrievedMaybeFetchInstallInfo(
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      blink::mojom::ManifestPtr opt_manifest,
      bool valid_manifest_for_web_app,
      webapps::InstallableStatusCode error_code);
  void OnIconsRetrievedForNoManifest(
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      IconsDownloadedResult result,
      IconsMap icons_map,
      DownloadedIconsHttpResults icons_http_results);
  void OnInstallInfoFetched(
      std::unique_ptr<WebAppInstallInfo> info_from_manifest);
  void CompleteCommandAndSelfDestruct(
      FetchInstallInfoResult result,
      std::unique_ptr<WebAppInstallInfo> install_info);

  std::unique_ptr<SharedWebContentsLock> lock_;

  const webapps::ManifestId manifest_id_;
  const GURL install_url_;
  const std::optional<webapps::ManifestId> parent_manifest_id_;

  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<ManifestToWebAppInstallInfoJob> manifest_to_install_info_job_;

  InstallErrorLogEntry install_error_log_entry_;

  base::WeakPtrFactory<FetchInstallInfoFromInstallUrlCommand> weak_ptr_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_INSTALL_INFO_FROM_INSTALL_URL_COMMAND_H_
