// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_UPDATE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_UPDATE_COMMAND_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_update_result.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom-forward.h"
#include "url/gurl.h"

namespace webapps {
enum class WebAppUrlLoaderResult;
}

namespace web_app {

class WebAppDataRetriever;
struct WebAppInstallInfo;

// This command will fetch the given install url with the shared web contents,
// and perform a manifest update, assuming that the loaded manifest's id matches
// the `expected_manifest_id`. This update is assumed to be for a trusted
// manifest, and all manifest icons will be saved as trusted icons.
//
// Side-effects:
// - This command will clear the PendingUpdateInfo if it exists on the web app,
//   and call the appropriate observer.
// - This command will call the OnWebAppManifestUpdated observer method.
class FetchManifestAndUpdateCommand
    : public WebAppCommand<SharedWebContentsLock,
                           FetchManifestAndUpdateResult> {
 public:
  FetchManifestAndUpdateCommand(const GURL& install_url,
                                const webapps::ManifestId& expected_manifest_id,
                                FetchManifestAndUpdateCallback callback);
  ~FetchManifestAndUpdateCommand() override;

  void StartWithLock(std::unique_ptr<SharedWebContentsLock> lock) override;

 private:
  void OnUrlLoaded(webapps::WebAppUrlLoaderResult result);
  void OnManifestRetrieved(
      const base::expected<blink::mojom::ManifestPtr,
                           blink::mojom::RequestManifestErrorPtr>& result);
  void OnAppLockAcquired();
  void OnWebAppInfoCreatedFromManifest(
      std::unique_ptr<WebAppInstallInfo> install_info);
  void OnIconsFetched();
  void OnUpdateFinalized(const webapps::AppId& app_id,
                         webapps::InstallResultCode code);

  const GURL install_url_;
  const webapps::ManifestId expected_manifest_id_;

  std::unique_ptr<SharedWebContentsLock> web_contents_lock_;
  std::unique_ptr<SharedWebContentsWithAppLock> app_lock_;
  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<WebAppInstallInfo> install_info_;

  std::unique_ptr<ManifestToWebAppInstallInfoJob> manifest_to_install_info_job_;

  base::WeakPtrFactory<FetchManifestAndUpdateCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_UPDATE_COMMAND_H_
