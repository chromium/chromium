// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_SYNC_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_SYNC_COMMAND_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

class Profile;

namespace webapps {
class WebAppUrlLoader;
enum class WebAppUrlLoaderResult;
}  // namespace webapps

namespace web_app {

class WebAppDataRetriever;

class InstallFromSyncCommand
    : public WebAppCommand<SharedWebContentsWithAppLock,
                           const webapps::AppId&,
                           webapps::InstallResultCode> {
 public:
  struct Params {
    Params() = delete;
    ~Params();
    Params(const Params&);
    Params(const webapps::AppId& app_id,
           const webapps::ManifestId& manifest_id,
           const GURL& start_url,
           const std::string& title,
           const GURL& scope,
           const std::optional<SkColor>& theme_color,
           const std::optional<mojom::UserDisplayMode>& user_display_mode,
           const std::vector<apps::IconInfo>& icons);
    const webapps::AppId app_id;
    const webapps::ManifestId manifest_id;
    const GURL start_url;
    const std::string title;
    const GURL scope;
    const std::optional<SkColor> theme_color;
    const std::optional<mojom::UserDisplayMode> user_display_mode;
    const std::vector<apps::IconInfo> icons;
  };
  using DataRetrieverFactory =
      base::RepeatingCallback<std::unique_ptr<WebAppDataRetriever>()>;

  InstallFromSyncCommand(Profile* profile,
                         const Params& params,
                         OnceInstallCallback install_callback);
  ~InstallFromSyncCommand() override;

  void SetFallbackTriggeredForTesting(
      base::OnceCallback<void(webapps::InstallResultCode code)> callback);

  // WebAppCommand:
 protected:
  void StartWithLock(
      std::unique_ptr<SharedWebContentsWithAppLock> lock) override;

 private:
  void OnWebAppUrlLoadedGetWebAppInstallInfo(
      webapps::WebAppUrlLoaderResult result);

  void OnGetWebAppInstallInfo(std::unique_ptr<WebAppInstallInfo> web_app_info);

  void OnDidPerformInstallableCheck(blink::mojom::ManifestPtr opt_manifest,
                                    bool valid_manifest_for_web_app,
                                    webapps::InstallableStatusCode error_code);

  enum class FinalizeMode { kNormalWebAppInfo, kFallbackWebAppInfo };

  void OnIconsRetrievedFinalizeInstall(
      FinalizeMode mode,
      IconsDownloadedResult result,
      IconsMap icons_map,
      DownloadedIconsHttpResults icons_http_results);

  void OnInstallFinalized(FinalizeMode mode,
                          const webapps::AppId& app_id,
                          webapps::InstallResultCode code);

  void InstallFallback(webapps::InstallResultCode error_code);

  void ReportResultAndDestroy(const webapps::AppId& app_id,
                              webapps::InstallResultCode code);

  std::unique_ptr<SharedWebContentsWithAppLock> lock_;

  const raw_ptr<Profile> profile_;
  const Params params_;

  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  std::unique_ptr<WebAppInstallInfo> install_info_;
  std::unique_ptr<WebAppInstallInfo> fallback_install_info_;

  InstallErrorLogEntry install_error_log_entry_;

  base::OnceCallback<void(webapps::InstallResultCode code)>
      fallback_triggered_for_testing_;

  base::WeakPtrFactory<InstallFromSyncCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_SYNC_COMMAND_H_
