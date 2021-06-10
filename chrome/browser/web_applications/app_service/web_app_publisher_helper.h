// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APP_PUBLISHER_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APP_PUBLISHER_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/icon_key_util.h"
#include "chrome/browser/apps/app_service/paused_apps.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/webapps/browser/installable/installable_metrics.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

class WebApp;
class WebAppProvider;
class WebAppRegistrar;
class WebAppLaunchManager;

class WebAppPublisherHelper : public AppRegistrarObserver,
                              public content_settings::Observer {
 public:
  class Delegate {
   public:
    Delegate();
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    ~Delegate();

    virtual void PublishWebApps(std::vector<apps::mojom::AppPtr> apps) = 0;
    virtual void PublishWebApp(apps::mojom::AppPtr app) = 0;
  };

  using LoadIconCallback = base::OnceCallback<void(apps::mojom::IconValuePtr)>;

  WebAppPublisherHelper(Profile* profile,
                        apps::mojom::AppType app_type,
                        Delegate* delegate);
  WebAppPublisherHelper(const WebAppPublisherHelper&) = delete;
  WebAppPublisherHelper& operator=(const WebAppPublisherHelper&) = delete;
  ~WebAppPublisherHelper() override;

  // Indicates if |permission_type| is supported by Web Applications.
  static bool IsSupportedWebAppPermissionType(
      ContentSettingsType permission_type);

  // Converts |uninstall_source| to a |WebappUninstallSource|.
  static webapps::WebappUninstallSource
  ConvertUninstallSourceToWebAppUninstallSource(
      apps::mojom::UninstallSource uninstall_source);

  // Returns true if the app is published as a web app.
  static bool Accepts(const std::string& app_id);

  // Must be called before profile keyed services are destroyed.
  void Shutdown();

  // Populates the various show_in_* fields of |app|.
  void SetWebAppShowInFields(apps::mojom::AppPtr& app, const WebApp* web_app);

  // Appends |web_app| permissions to |target|.
  void PopulateWebAppPermissions(
      const WebApp* web_app,
      std::vector<apps::mojom::PermissionPtr>* target);

  // Creates an |apps::mojom::App| describing |web_app|.
  apps::mojom::AppPtr ConvertWebApp(const WebApp* web_app,
                                    apps::mojom::Readiness readiness);

  // Constructs an App with only the information required to identify an
  // uninstallation.
  apps::mojom::AppPtr ConvertUninstalledWebApp(const WebApp* web_app);

  // Constructs an App with only the information required to update
  // last launch time.
  apps::mojom::AppPtr ConvertLaunchedWebApp(const WebApp* web_app);

  // Directly uninstalls |web_app| without prompting the user.
  // If |clear_site_data| is true, any site data associated with the app will
  // be removed.
  // If |report_abuse| is true, the app will be reported for abuse to the Web
  // Store.
  void UninstallWebApp(const WebApp* web_app,
                       apps::mojom::UninstallSource uninstall_source,
                       bool clear_site_data,
                       bool report_abuse);

  // If |is_disabled| is |absl::nullopt|, |web_app->chromos_data().is_disabled|
  // is consulted instead.
  apps::mojom::IconKeyPtr MakeIconKey(
      const WebApp* web_app,
      absl::optional<bool> is_disabled = absl::nullopt);

  void SetIconEffect(const std::string& app_id);

  void PauseApp(const std::string& app_id);

  void UnpauseApp(const std::string& app_id);

  bool IsPaused(const std::string& app_id);

  void MaybeRemovePausedApp(const std::string& app_id);

  void LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback);

  content::WebContents* Launch(const std::string& app_id,
                               int32_t event_flags,
                               apps::mojom::LaunchSource launch_source,
                               apps::mojom::WindowInfoPtr window_info);

  content::WebContents* LaunchAppWithFiles(
      const std::string& app_id,
      apps::mojom::LaunchContainer container,
      int32_t event_flags,
      apps::mojom::LaunchSource launch_source,
      apps::mojom::FilePathsPtr file_paths);

  content::WebContents* LaunchAppWithIntent(
      const std::string& app_id,
      int32_t event_flags,
      apps::mojom::IntentPtr intent,
      apps::mojom::LaunchSource launch_source,
      apps::mojom::WindowInfoPtr window_info);

  content::WebContents* LaunchAppWithParams(apps::AppLaunchParams params);

  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission);

  void OpenNativeSettings(const std::string& app_id);

  void SetWindowMode(const std::string& app_id,
                     apps::mojom::WindowMode window_mode);

  // Converts |display_mode| to a |window_mode|.
  apps::mojom::WindowMode ConvertDisplayModeToWindowMode(
      blink::mojom::DisplayMode display_mode,
      bool in_experimental_tabbed_window);

  void PublishWindowModeUpdate(const std::string& app_id,
                               blink::mojom::DisplayMode display_mode,
                               bool in_experimental_tabbed_window);

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/1194709): Make this method private, move
  // WebAppsChromeOs::OnWebAppInstalled() logic into this class.
  // Checks whether the |app_id| is in the disabled list.
  bool IsWebAppInDisabledList(const std::string& app_id) const;
#endif

  Profile* profile() { return profile_; }

  apps::mojom::AppType app_type() const { return app_type_; }

  WebAppRegistrar& registrar() const;

 private:
  // AppRegistrarObserver:
  void OnAppRegistrarDestroyed() override;
  void OnWebAppLocallyInstalledStateChanged(const AppId& app_id,
                                            bool is_locally_installed) override;
  void OnWebAppLastLaunchTimeChanged(
      const std::string& app_id,
      const base::Time& last_launch_time) override;
  void OnWebAppUserDisplayModeChanged(const AppId& app_id,
                                      DisplayMode user_display_mode) override;
  void OnWebAppExperimentalTabbedWindowModeChanged(const AppId& app_id,
                                                   bool enabled) override;
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  void OnWebAppDisabledStateChanged(const AppId& app_id,
                                    bool is_disabled) override;
  void OnWebAppsDisabledModeChanged() override;
#endif

  // content_settings::Observer:
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type) override;

  void Init();

  apps::IconEffects GetIconEffects(const WebApp* web_app,
                                   absl::optional<bool> is_disabled_opt);

  const WebApp* GetWebApp(const AppId& app_id) const;

  content::WebContents* LaunchAppWithIntentImpl(
      const std::string& app_id,
      int32_t event_flags,
      apps::mojom::IntentPtr intent,
      apps::mojom::LaunchSource launch_source,
      int64_t display_id);

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Updates app visibility.
  void UpdateAppDisabledMode(apps::mojom::AppPtr& app);
#endif

  Profile* const profile_;

  // The app type of the publisher. The app type is kSystemWeb if the web apps
  // are serving from Lacros, and the app type is kWeb for all other cases.
  const apps::mojom::AppType app_type_;

  Delegate* const delegate_;

  WebAppProvider* const provider_;

  base::ScopedObservation<AppRegistrar, AppRegistrarObserver>
      registrar_observation_{this};

  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observation_{this};

  std::unique_ptr<WebAppLaunchManager> web_app_launch_manager_;

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  apps::PausedApps paused_apps_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APP_PUBLISHER_HELPER_H_
