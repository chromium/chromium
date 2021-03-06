// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;
class Profile;
struct WebApplicationInfo;

namespace blink {
struct Manifest;
}

namespace content {
class WebContents;
}

namespace web_app {

class OsIntegrationManager;
class InstallFinalizer;
class WebAppDataRetriever;
class WebAppUrlLoader;
class AppRegistrar;

// Used to do a variety of tasks involving installing web applications. Only one
// of the public Load*, Update*, or Install* methods can be called on a single
// object. WebAppInstallManager is a queue of WebAppInstallTask jobs. Basically,
// WebAppInstallTask is an implementation detail of WebAppInstallManager.
class WebAppInstallTask : content::WebContentsObserver {
 public:
  using RetrieveWebApplicationInfoWithIconsCallback =
      base::OnceCallback<void(std::unique_ptr<WebApplicationInfo>)>;

  WebAppInstallTask(Profile* profile,
                    OsIntegrationManager* os_integration_manager,
                    InstallFinalizer* install_finalizer,
                    std::unique_ptr<WebAppDataRetriever> data_retriever,
                    AppRegistrar* registrar);
  WebAppInstallTask(const WebAppInstallTask&) = delete;
  WebAppInstallTask& operator=(const WebAppInstallTask&) = delete;
  ~WebAppInstallTask() override;

  // Request the app_id expectation check. Install fails with
  // kExpectedAppIdCheckFailed if actual app_id doesn't match expected app_id.
  // The actual resulting app_id is reported as a part of OnceInstallCallback.
  void ExpectAppId(const AppId& expected_app_id);
  const base::Optional<AppId>& app_id_to_expect() const {
    return expected_app_id_;
  }

  void SetInstallParams(const InstallManager::InstallParams& install_params);

  using LoadWebAppAndCheckManifestCallback = base::OnceCallback<void(
      std::unique_ptr<content::WebContents> web_contents,
      const AppId& app_id,
      InstallResultCode code)>;
  // Load a web app from the given URL and check for valid manifest.
  void LoadWebAppAndCheckManifest(const GURL& url,
                                  webapps::WebappInstallSource install_source,
                                  WebAppUrlLoader* url_loader,
                                  LoadWebAppAndCheckManifestCallback callback);

  // Checks a WebApp installability, retrieves manifest and icons and
  // then performs the actual installation.
  void InstallWebAppFromManifest(
      content::WebContents* web_contents,
      bool bypass_service_worker_check,
      webapps::WebappInstallSource install_source,
      InstallManager::WebAppInstallDialogCallback dialog_callback,
      InstallManager::OnceInstallCallback callback);

  // This method infers WebApp info from the blink renderer process
  // and then retrieves a manifest in a way similar to
  // |InstallWebAppFromManifest|. If manifest is incomplete or missing, the
  // inferred info is used.
  void InstallWebAppFromManifestWithFallback(
      content::WebContents* web_contents,
      bool force_shortcut_app,
      webapps::WebappInstallSource install_source,
      InstallManager::WebAppInstallDialogCallback dialog_callback,
      InstallManager::OnceInstallCallback callback);

  // Load |launch_url| and do silent background install with
  // |InstallWebAppFromManifestWithFallback|. Posts |LoadUrl| task to
  // |url_loader| immediately. Doesn't memorize |url_loader| pointer.
  void LoadAndInstallWebAppFromManifestWithFallback(
      const GURL& launch_url,
      content::WebContents* web_contents,
      WebAppUrlLoader* url_loader,
      webapps::WebappInstallSource install_source,
      InstallManager::OnceInstallCallback callback);

  // Fetches the icon URLs in |web_application_info| to populate the icon
  // bitmaps. Once fetched uses the contents of |web_application_info| as the
  // entire web app installation data.
  void InstallWebAppFromInfoRetrieveIcons(
      content::WebContents* web_contents,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      InstallFinalizer::FinalizeOptions finalize_options,
      InstallManager::OnceInstallCallback callback);

  // Starts a web app installation process using prefilled
  // |web_application_info| which holds all the data needed for installation.
  // InstallManager doesn't fetch a manifest.
  void InstallWebAppFromInfo(
      std::unique_ptr<WebApplicationInfo> web_application_info,
      ForInstallableSite for_installable_site,
      webapps::WebappInstallSource install_source,
      InstallManager::OnceInstallCallback callback);

  // Starts a background web app installation process for a given
  // |web_contents|. This method infers WebApp info from the blink renderer
  // process and then retrieves a manifest in a way similar to
  // |InstallWebAppFromManifestWithFallback|.
  void InstallWebAppWithParams(
      content::WebContents* web_contents,
      const InstallManager::InstallParams& install_params,
      webapps::WebappInstallSource install_source,
      InstallManager::OnceInstallCallback callback);

  void UpdateWebAppFromInfo(
      content::WebContents* web_contents,
      const AppId& app_id,
      std::unique_ptr<WebApplicationInfo> web_application_info,
      bool redownload_app_icons,
      InstallManager::OnceInstallCallback callback);

  // Obtains WebApplicationInfo about web app located at |start_url|, fallbacks
  // to title/favicon if manifest is not present.
  void LoadAndRetrieveWebApplicationInfoWithIcons(
      const GURL& start_url,
      WebAppUrlLoader* url_loader,
      RetrieveWebApplicationInfoWithIconsCallback callback);

  static std::unique_ptr<content::WebContents> CreateWebContents(
      Profile* profile);

  base::WeakPtr<WebAppInstallTask> GetWeakPtr();

  // WebContentsObserver:
  void WebContentsDestroyed() override;

  void SetInstallFinalizerForTesting(InstallFinalizer* install_finalizer);

 private:
  void CheckInstallPreconditions();
  void RecordInstallEvent();

  // Calling the callback may destroy |this| task. Callers shouldn't work with
  // any |this| class members after calling it.
  void CallInstallCallback(const AppId& app_id, InstallResultCode code);

  // Checks if any errors occurred while |this| was async awaiting. All On*
  // completion handlers below must return early if this is true. Also, if
  // ShouldStopInstall is true, install_callback_ is already invoked or may be
  // invoked later: All On* completion handlers don't need to call
  // install_callback_.
  bool ShouldStopInstall() const;

  void OnWebAppUrlLoadedGetWebApplicationInfo(WebAppUrlLoader::Result result);

  void OnWebAppUrlLoadedCheckAndRetrieveManifest(
      content::WebContents* web_contents,
      WebAppUrlLoader::Result result);
  void OnWebAppInstallabilityChecked(
      base::Optional<blink::Manifest> opt_manifest,
      const GURL& manifest_url,
      bool valid_manifest_for_web_app,
      bool is_installable);

  void OnGetWebApplicationInfo(
      bool force_shortcut_app,
      std::unique_ptr<WebApplicationInfo> web_app_info);

  // Makes amendments to |web_app_info| based on the options set in
  // |install_params|.
  void ApplyParamsToWebApplicationInfo(
      const InstallManager::InstallParams& install_params,
      WebApplicationInfo& web_app_info);

  void OnDidPerformInstallableCheck(
      std::unique_ptr<WebApplicationInfo> web_app_info,
      bool force_shortcut_app,
      base::Optional<blink::Manifest> opt_manifest,
      const GURL& manifest_url,
      bool valid_manifest_for_web_app,
      bool is_installable);

  // Either dispatches an asynchronous check for whether this installation
  // should be stopped and an intent to the Play Store should be made, or
  // synchronously calls OnDidCheckForIntentToPlayStore() implicitly failing the
  // check if it cannot be made.
  void CheckForPlayStoreIntentOrGetIcons(
      base::Optional<blink::Manifest> opt_manifest,
      std::unique_ptr<WebApplicationInfo> web_app_info,
      std::vector<GURL> icon_urls,
      ForInstallableSite for_installable_site,
      bool skip_page_favicons);

  // Called when the asynchronous check for whether an intent to the Play Store
  // should be made returns.
  void OnDidCheckForIntentToPlayStore(
      std::unique_ptr<WebApplicationInfo> web_app_info,
      std::vector<GURL> icon_urls,
      ForInstallableSite for_installable_site,
      bool skip_page_favicons,
      const std::string& intent,
      bool should_intent_to_store);

  void OnIconsRetrieved(std::unique_ptr<WebApplicationInfo> web_app_info,
                        InstallFinalizer::FinalizeOptions finalize_options,
                        IconsMap icons_map);
  void OnIconsRetrievedShowDialog(
      std::unique_ptr<WebApplicationInfo> web_app_info,
      ForInstallableSite for_installable_site,
      IconsMap icons_map);
  void OnIconsRetrievedFinalizeUpdate(
      std::unique_ptr<WebApplicationInfo> web_app_info,
      bool redownload_app_icons,
      IconsMap icons_map);
  void OnDialogCompleted(ForInstallableSite for_installable_site,
                         bool user_accepted,
                         std::unique_ptr<WebApplicationInfo> web_app_info);
  void OnInstallFinalized(const AppId& app_id, InstallResultCode code);
  void OnInstallFinalizedCreateShortcuts(
      std::unique_ptr<WebApplicationInfo> web_app_info,
      const AppId& app_id,
      InstallResultCode code);
  void OnOsHooksCreated(bool open_as_window,
                        const AppId& app_id,
                        const OsHooksResults os_hooks_results);

  // Whether the install task has been 'initiated' by calling one of the public
  // methods.
  bool initiated_ = false;

  // Whether we should just obtain WebApplicationInfo instead of the actual
  // installation.
  bool only_retrieve_web_application_info_ = false;

  InstallManager::WebAppInstallDialogCallback dialog_callback_;
  InstallManager::OnceInstallCallback install_callback_;
  RetrieveWebApplicationInfoWithIconsCallback retrieve_info_callback_;
  base::Optional<InstallManager::InstallParams> install_params_;
  base::Optional<AppId> expected_app_id_;
  bool background_installation_ = false;

  // The mechanism via which the app creation was triggered, will stay as
  // kNoInstallSource for updates.
  static constexpr webapps::WebappInstallSource kNoInstallSource =
      webapps::WebappInstallSource::COUNT;
  webapps::WebappInstallSource install_source_ = kNoInstallSource;

  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<WebApplicationInfo> web_application_info_;
  std::unique_ptr<content::WebContents> web_contents_;

  OsIntegrationManager* os_integration_manager_;
  InstallFinalizer* install_finalizer_;
  Profile* const profile_;
  AppRegistrar* registrar_;

  base::WeakPtrFactory<WebAppInstallTask> weak_ptr_factory_{this};

};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_TASK_H_
