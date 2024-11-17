// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_INSTALL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_INSTALL_COMMAND_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_logging.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/arc.mojom.h"
#endif

namespace content {
class WebContents;
class NavigationHandle;
}  // namespace content

namespace web_app {

class AppLock;
class WebAppDataRetriever;

// Install web app from manifest for current `WebContents`.
class FetchManifestAndInstallCommand
    : public WebAppCommand<NoopLock,
                           const webapps::AppId&,
                           webapps::InstallResultCode>,
      public content::WebContentsObserver {
 public:
  // `use_fallback` allows getting fallback information from current document
  // to enable installing a non-promotable site.
  FetchManifestAndInstallCommand(webapps::WebappInstallSource install_surface,
                                 base::WeakPtr<content::WebContents> contents,
                                 WebAppInstallDialogCallback dialog_callback,
                                 OnceInstallCallback callback,
                                 FallbackBehavior behavior,
                                 base::WeakPtr<WebAppUiManager> ui_manager);

  ~FetchManifestAndInstallCommand() override;

  // WebAppCommand:
  void OnShutdown(base::PassKey<WebAppCommandManager>) const override;
  content::WebContents* GetInstallingWebContents(
      base::PassKey<WebAppCommandManager>) override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<NoopLock> lock) override;

 private:
  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

  void Abort(webapps::InstallResultCode code,
             const base::Location& location = FROM_HERE);
  bool IsWebContentsDestroyed();

  void FetchFallbackInstallInfo();
  void OnGetWebAppInstallInfo(
      std::unique_ptr<WebAppInstallInfo> fallback_web_app_info);
  void FetchManifest();
  void OnDidPerformInstallableCheck(blink::mojom::ManifestPtr opt_manifest,
                                    bool valid_manifest_for_web_app,
                                    webapps::InstallableStatusCode error_code);

  // Either dispatches an asynchronous check for whether this installation
  // should be stopped and an intent to the Play Store should be made, or
  // synchronously calls OnDidCheckForIntentToPlayStore() implicitly failing the
  // check if it cannot be made.
  void CheckForPlayStoreIntentOrGetIcons();

  // Called when the asynchronous check for whether an intent to the Play Store
  // should be made returns.
  void OnDidCheckForIntentToPlayStore(const std::string& intent,
                                      bool should_intent_to_store);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Called when the asynchronous check for whether an intent to the Play Store
  // should be made returns (Lacros adapter that calls
  // |OnDidCheckForIntentToPlayStore| based on |result|).
  void OnDidCheckForIntentToPlayStoreLacros(
      const std::string& intent,
      crosapi::mojom::IsInstallableResult result);
#endif

  void OnIconsRetrievedShowDialog(
      IconsDownloadedResult result,
      IconsMap icons_map,
      DownloadedIconsHttpResults icons_http_results);
  void OnDialogCompleted(bool user_accepted,
                         std::unique_ptr<WebAppInstallInfo> web_app_info);
  void OnInstallFinalizedMaybeReparentTab(const webapps::AppId& app_id,
                                          webapps::InstallResultCode code);

  void OnInstallCompleted(const webapps::AppId& app_id,
                          webapps::InstallResultCode code);
  void MeasureUserInstalledAppHistogram(webapps::InstallResultCode code);

  const webapps::WebappInstallSource install_surface_;
  const base::WeakPtr<content::WebContents> web_contents_;
  WebAppInstallDialogCallback dialog_callback_;
  const FallbackBehavior fallback_behavior_;
  const base::WeakPtr<WebAppUiManager> ui_manager_;

  std::unique_ptr<NoopLock> noop_lock_;
  std::unique_ptr<AppLock> app_lock_;

  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  bool did_navigation_occur_before_start_ = false;

  InstallErrorLogEntry install_error_log_entry_;

  std::unique_ptr<WebAppInstallInfo> web_app_info_;
  blink::mojom::ManifestPtr opt_manifest_;
  bool valid_manifest_for_crafted_web_app_ = false;
  IconUrlSizeSet icons_from_manifest_;
  bool skip_page_favicons_on_initial_download_ = false;

  base::WeakPtrFactory<FetchManifestAndInstallCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_INSTALL_COMMAND_H_
