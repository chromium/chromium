// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/arc.mojom-forward.h"
#endif

class GURL;
class Profile;
struct WebAppInstallInfo;

namespace content {
class WebContents;
}

namespace web_app {

class WebAppDataRetriever;
class WebAppUrlLoader;
class WebAppRegistrar;

// Used to do a variety of tasks involving installing web applications. Only one
// of the public Load*, Update*, or Install* methods can be called on a single
// object. WebAppInstallManager is a queue of WebAppInstallTask jobs. Basically,
// WebAppInstallTask is an implementation detail of WebAppInstallManager.
class WebAppInstallTask : content::WebContentsObserver {
 public:
  using WebAppInstallInfoOrErrorCode =
      absl::variant<WebAppInstallInfo, webapps::InstallResultCode>;
  using RetrieveWebAppInstallInfoWithIconsCallback =
      base::OnceCallback<void(WebAppInstallInfoOrErrorCode)>;

  using WebAppInstallFlow = WebAppInstallManager::WebAppInstallFlow;

  WebAppInstallTask(Profile* profile,
                    WebAppInstallManager* install_manager,
                    WebAppInstallFinalizer* install_finalizer,
                    std::unique_ptr<WebAppDataRetriever> data_retriever,
                    WebAppRegistrar* registrar,
                    webapps::WebappInstallSource install_surface);
  WebAppInstallTask(const WebAppInstallTask&) = delete;
  WebAppInstallTask& operator=(const WebAppInstallTask&) = delete;
  ~WebAppInstallTask() override;

  // Request the app_id expectation check. Install fails with
  // kExpectedAppIdCheckFailed if actual app_id doesn't match expected app_id.
  // The actual resulting app_id is reported as a part of OnceInstallCallback.
  void ExpectAppId(const AppId& expected_app_id);
  const absl::optional<AppId>& app_id_to_expect() const {
    return expected_app_id_;
  }

  void SetInstallParams(const WebAppInstallParams& install_params);

  using LoadWebAppAndCheckManifestCallback = base::OnceCallback<void(
      std::unique_ptr<content::WebContents> web_contents,
      const AppId& app_id,
      webapps::InstallResultCode code)>;
  // Load a web app from the given URL and check for valid manifest.
  void LoadWebAppAndCheckManifest(const GURL& url,
                                  WebAppUrlLoader* url_loader,
                                  LoadWebAppAndCheckManifestCallback callback);

  // Checks a WebApp installability, retrieves manifest and icons and
  // then performs the actual installation.
  void InstallWebAppFromManifest(content::WebContents* web_contents,
                                 bool bypass_service_worker_check,
                                 WebAppInstallDialogCallback dialog_callback,
                                 OnceInstallCallback callback);

  // This method infers WebApp info from the blink renderer process
  // and then retrieves a manifest in a way similar to
  // |InstallWebAppFromManifest|. If manifest is incomplete or missing, the
  // inferred info is used.
  void InstallWebAppFromManifestWithFallback(
      content::WebContents* web_contents,
      WebAppInstallFlow flow,
      WebAppInstallDialogCallback dialog_callback,
      OnceInstallCallback callback);

  // Load |launch_url| and do silent background install with
  // |InstallWebAppFromManifestWithFallback|. Posts |LoadUrl| task to
  // |url_loader| immediately. Doesn't memorize |url_loader| pointer.
  void LoadAndInstallWebAppFromManifestWithFallback(
      const GURL& launch_url,
      content::WebContents* web_contents,
      WebAppUrlLoader* url_loader,
      OnceInstallCallback callback);

  // Load |install_url| and install SubApp. Posts |LoadUrl| task to |url_loader|
  // immediately. Doesn't memorize |url_loader| pointer.
  void LoadAndInstallSubAppFromURL(const GURL& install_url,
                                   content::WebContents* contents,
                                   WebAppUrlLoader* url_loader,
                                   OnceInstallCallback install_callback);

  // Fetches the icon URLs in |web_application_info| to populate the icon
  // bitmaps. Once fetched uses the contents of |web_application_info| as the
  // entire web app installation data.
  void InstallWebAppFromInfoRetrieveIcons(
      content::WebContents* web_contents,
      std::unique_ptr<WebAppInstallInfo> web_application_info,
      WebAppInstallFinalizer::FinalizeOptions finalize_options,
      OnceInstallCallback callback);

  // Starts a web app installation process using prefilled
  // |web_application_info| which holds all the data needed for installation.
  // WebAppInstallManager doesn't fetch a manifest.
  void InstallWebAppFromInfo(
      std::unique_ptr<WebAppInstallInfo> web_application_info,
      bool overwrite_existing_manifest_fields,
      OnceInstallCallback callback);

  // Starts a background web app installation process for a given
  // |web_contents|. This method infers WebApp info from the blink renderer
  // process and then retrieves a manifest in a way similar to
  // |InstallWebAppFromManifestWithFallback|.
  void InstallWebAppWithParams(content::WebContents* web_contents,
                               const WebAppInstallParams& install_params,
                               OnceInstallCallback callback);

  // Obtains WebAppInstallInfo about web app located at |start_url|, fallbacks
  // to title/favicon if manifest is not present.
  void LoadAndRetrieveWebAppInstallInfoWithIcons(
      const GURL& start_url,
      WebAppUrlLoader* url_loader,
      RetrieveWebAppInstallInfoWithIconsCallback callback);

  static std::unique_ptr<content::WebContents> CreateWebContents(
      Profile* profile);

  // Returns the pre-existing web contents the installation was initiated with,
  // or the one created specifically for the install task.
  content::WebContents* GetInstallingWebContents();

  base::WeakPtr<WebAppInstallTask> GetWeakPtr();

  // WebContentsObserver:
  void WebContentsDestroyed() override;

  // Collects install errors (unbounded) if the |kRecordWebAppDebugInfo|
  // flag is enabled to be used by: chrome://web-app-internals
  base::Value TakeErrorDict();

  void SetInstallFinalizerForTesting(
      WebAppInstallFinalizer* install_finalizer) {
    install_finalizer_ = install_finalizer;
  }

  void SetFlowForTesting(WebAppInstallFlow flow) { flow_ = flow; }

 private:
  void CheckInstallPreconditions();
  void RecordInstallEvent();

  // Calling the callback may destroy |this| task. Callers shouldn't work with
  // any |this| class members after calling it.
  void CallInstallCallback(const AppId& app_id,
                           webapps::InstallResultCode code);

  // Checks if any errors occurred while |this| was async awaiting. All On*
  // completion handlers below must return early if this is true. Also, if
  // ShouldStopInstall is true, install_callback_ is already invoked or may be
  // invoked later: All On* completion handlers don't need to call
  // install_callback_.
  bool ShouldStopInstall() const;

  void OnWebAppUrlLoadedGetWebAppInstallInfo(const GURL& url_to_load,
                                             WebAppUrlLoader::Result result);

  void OnWebAppUrlLoadedCheckAndRetrieveManifest(
      const GURL& url_to_load,
      content::WebContents* web_contents,
      WebAppUrlLoader::Result result);
  void OnWebAppInstallabilityChecked(blink::mojom::ManifestPtr opt_manifest,
                                     const GURL& manifest_url,
                                     bool valid_manifest_for_web_app,
                                     bool is_installable);

  void OnGetWebAppInstallInfo(std::unique_ptr<WebAppInstallInfo> web_app_info);

  // Makes amendments to |web_app_info| based on the options set in
  // |install_params|.
  void ApplyParamsToWebAppInstallInfo(const WebAppInstallParams& install_params,
                                      WebAppInstallInfo& web_app_info);

  void OnDidPerformInstallableCheck(
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      blink::mojom::ManifestPtr opt_manifest,
      const GURL& manifest_url,
      bool valid_manifest_for_web_app,
      bool is_installable);

  // Either dispatches an asynchronous check for whether this installation
  // should be stopped and an intent to the Play Store should be made, or
  // synchronously calls OnDidCheckForIntentToPlayStore() implicitly failing the
  // check if it cannot be made.
  void CheckForPlayStoreIntentOrGetIcons(
      blink::mojom::ManifestPtr opt_manifest,
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      std::vector<GURL> icon_urls,
      bool skip_page_favicons);

  // Called when the asynchronous check for whether an intent to the Play Store
  // should be made returns.
  void OnDidCheckForIntentToPlayStore(
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      std::vector<GURL> icon_urls,
      bool skip_page_favicons,
      const std::string& intent,
      bool should_intent_to_store);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Called when the asynchronous check for whether an intent to the Play Store
  // should be made returns (Lacros adapter that calls
  // |OnDidCheckForIntentToPlayStore| based on |result|).
  void OnDidCheckForIntentToPlayStoreLacros(
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      std::vector<GURL> icon_urls,
      bool skip_page_favicons,
      const std::string& intent,
      crosapi::mojom::IsInstallableResult result);
#endif

  void OnIconsRetrieved(
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      WebAppInstallFinalizer::FinalizeOptions finalize_options,
      IconsDownloadedResult result,
      IconsMap icons_map,
      DownloadedIconsHttpResults icons_http_results);
  void OnIconsRetrievedShowDialog(
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      IconsDownloadedResult result,
      IconsMap icons_map,
      DownloadedIconsHttpResults icons_http_results);
  void OnDialogCompleted(bool user_accepted,
                         std::unique_ptr<WebAppInstallInfo> web_app_info);
  void OnInstallFinalized(const AppId& app_id,
                          webapps::InstallResultCode code,
                          OsHooksErrors os_hooks_errors);
  void OnInstallFinalizedMaybeReparentTab(
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      const AppId& app_id,
      webapps::InstallResultCode code,
      OsHooksErrors os_hooks_errors);
  void OnOsHooksCreated(DisplayMode user_display_mode,
                        const AppId& app_id,
                        OsHooksErrors os_hook_errors);

  void RecordDownloadedIconsResultAndHttpStatusCodes(
      IconsDownloadedResult result,
      const DownloadedIconsHttpResults& icons_http_results);

  void LogHeaderIfLogEmpty(const std::string& url);
  void LogErrorObject(const char* stage,
                      const std::string& url,
                      base::Value object);

  void LogUrlLoaderError(const char* stage,
                         const std::string& url,
                         WebAppUrlLoader::Result result);
  void LogExpectedAppIdError(const char* stage,
                             const std::string& url,
                             const AppId& app_id);
  void LogDownloadedIconsErrors(
      const WebAppInstallInfo& web_app_info,
      IconsDownloadedResult icons_downloaded_result,
      const IconsMap& icons_map,
      const DownloadedIconsHttpResults& icons_http_results);

  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  raw_ptr<WebAppInstallManager> install_manager_;
  raw_ptr<WebAppInstallFinalizer> install_finalizer_;
  const raw_ptr<Profile> profile_;
  raw_ptr<WebAppRegistrar> registrar_;

  webapps::WebappInstallSource install_surface_;

  // Whether the install task has been 'initiated' by calling one of the public
  // methods.
  bool initiated_ = false;

  // Whether we should just obtain WebAppInstallInfo instead of the actual
  // installation.
  bool only_retrieve_web_application_info_ = false;

  WebAppInstallDialogCallback dialog_callback_;
  OnceInstallCallback install_callback_;
  RetrieveWebAppInstallInfoWithIconsCallback retrieve_info_callback_;
  absl::optional<WebAppInstallParams> install_params_;
  absl::optional<AppId> expected_app_id_;
  bool background_installation_ = false;

  absl::optional<WebAppInstallInfo> web_application_info_;
  std::unique_ptr<content::WebContents> web_contents_;

  std::unique_ptr<base::Value> error_dict_;

  // TODO(crbug.com/1216457): Make this enum const and set its value in the
  // constructor.
  WebAppInstallFlow flow_ = WebAppInstallFlow::kUnknown;

  base::WeakPtrFactory<WebAppInstallTask> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_
