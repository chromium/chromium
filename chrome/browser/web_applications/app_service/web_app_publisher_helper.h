// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APP_PUBLISHER_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APP_PUBLISHER_HELPER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/id_type.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/icon_key_util.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/paused_apps.h"
#include "chrome/browser/web_applications/app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/apps/app_service/app_notifications.h"
#include "chrome/browser/apps/app_service/app_web_contents_data.h"
#include "chrome/browser/apps/app_service/media_requests.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_delegate.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "ui/message_center/public/cpp/notification.h"
#endif

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

class WebApp;
class WebAppProvider;
class WebAppRegistrar;
class WebAppLaunchManager;
class LinkCapturingMigrationManager;

struct ShortcutIdTypeMarker {};

typedef base::IdTypeU32<ShortcutIdTypeMarker> ShortcutId;

void UninstallImpl(WebAppProvider* provider,
                   const std::string& app_id,
                   apps::mojom::UninstallSource uninstall_source,
                   gfx::NativeWindow parent_window);

class WebAppPublisherHelper : public AppRegistrarObserver,
#if defined(OS_CHROMEOS)
                              public NotificationDisplayService::Observer,
                              public MediaCaptureDevicesDispatcher::Observer,
                              public apps::AppWebContentsData::Client,
#endif
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

    virtual void ModifyWebAppCapabilityAccess(
        const std::string& app_id,
        absl::optional<bool> accessing_camera,
        absl::optional<bool> accessing_microphone) = 0;
  };

  using LoadIconCallback = base::OnceCallback<void(apps::IconValuePtr)>;

  WebAppPublisherHelper(Profile* profile,
                        WebAppProvider* provider,
                        apps::mojom::AppType app_type,
                        Delegate* delegate,
                        bool observe_media_requests);
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

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  // Creates an |std::unique_ptr<apps::App>| describing |web_app|.
  std::unique_ptr<apps::App> CreateWebApp(const WebApp* web_app);
#endif

  // Creates an |apps::mojom::App| describing |web_app|.
  apps::mojom::AppPtr ConvertWebApp(const WebApp* web_app);

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

  apps::mojom::IconKeyPtr MakeIconKey(const WebApp* web_app);

  void SetIconEffect(const std::string& app_id);

  void PauseApp(const std::string& app_id);

  void UnpauseApp(const std::string& app_id);

  bool IsPaused(const std::string& app_id);

  void LoadIcon(const std::string& app_id,
                apps::IconType icon_type,
                int32_t size_hint_in_dip,
                apps::IconEffects icon_effects,
                LoadIconCallback callback);

  content::WebContents* Launch(const std::string& app_id,
                               int32_t event_flags,
                               apps::mojom::LaunchSource launch_source,
                               apps::mojom::WindowInfoPtr window_info);

  void LaunchAppWithFiles(const std::string& app_id,
                          int32_t event_flags,
                          apps::mojom::LaunchSource launch_source,
                          apps::mojom::FilePathsPtr file_paths);

  content::WebContents* MaybeNavigateExistingWindow(const std::string& app_id,
                                                    absl::optional<GURL> url);

  void LaunchAppWithIntent(
      const std::string& app_id,
      int32_t event_flags,
      apps::mojom::IntentPtr intent,
      apps::mojom::LaunchSource launch_source,
      apps::mojom::WindowInfoPtr window_info,
      apps::mojom::Publisher::LaunchAppWithIntentCallback callback);

  content::WebContents* LaunchAppWithParams(apps::AppLaunchParams params);

  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission);

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  void StopApp(const std::string& app_id);
#endif

  void OpenNativeSettings(const std::string& app_id);

  apps::mojom::WindowMode GetWindowMode(const std::string& app_id);

  void SetWindowMode(const std::string& app_id,
                     apps::mojom::WindowMode window_mode);

  // Converts |display_mode| to a |window_mode|.
  apps::mojom::WindowMode ConvertDisplayModeToWindowMode(
      blink::mojom::DisplayMode display_mode);

  void PublishWindowModeUpdate(const std::string& app_id,
                               blink::mojom::DisplayMode display_mode);

  std::string GenerateShortcutId();

  void StoreShortcutId(const std::string& shortcut_id,
                       const WebAppShortcutsMenuItemInfo& menu_item_info);

  // Execute the user command from the context menu items. Currently
  // on the web app shortcut need to be execute in the publisher.
  // The |app_id| represent the app that user selected, the |shortcut_id|
  // represents which shortcut item that user selected. The |display_id|
  // represent where to display the app.
  content::WebContents* ExecuteContextMenuCommand(
      const std::string& app_id,
      const std::string& shortcut_id,
      int64_t display_id);

  Profile* profile() { return profile_; }

  apps::mojom::AppType app_type() const { return app_type_; }

  WebAppRegistrar& registrar() const;

  bool IsShuttingDown() const;

 private:
#if defined(OS_CHROMEOS)
  class BadgeManagerDelegate : public badging::BadgeManagerDelegate {
   public:
    explicit BadgeManagerDelegate(
        const base::WeakPtr<WebAppPublisherHelper>& publisher_helper);

    ~BadgeManagerDelegate() override;

    void OnAppBadgeUpdated(const AppId& app_id) override;

   private:
    base::WeakPtr<WebAppPublisherHelper> publisher_helper_;
  };
#endif

  // AppRegistrarObserver:
  void OnWebAppInstalled(const AppId& app_id) override;
  void OnWebAppInstalledWithOsHooks(const AppId& app_id) override;
  void OnWebAppManifestUpdated(const AppId& app_id,
                               base::StringPiece old_name) override;
  void OnWebAppWillBeUninstalled(const AppId& app_id) override;
  void OnAppRegistrarDestroyed() override;
  void OnWebAppLocallyInstalledStateChanged(const AppId& app_id,
                                            bool is_locally_installed) override;
  void OnWebAppLastLaunchTimeChanged(
      const std::string& app_id,
      const base::Time& last_launch_time) override;
  void OnWebAppUserDisplayModeChanged(const AppId& app_id,
                                      DisplayMode user_display_mode) override;
#if defined(OS_CHROMEOS)
  void OnWebAppDisabledStateChanged(const AppId& app_id,
                                    bool is_disabled) override;
  void OnWebAppsDisabledModeChanged() override;

  // NotificationDisplayService::Observer overrides.
  void OnNotificationDisplayed(
      const message_center::Notification& notification,
      const NotificationCommon::Metadata* const metadata) override;
  void OnNotificationClosed(const std::string& notification_id) override;
  void OnNotificationDisplayServiceDestroyed(
      NotificationDisplayService* service) override;
#endif

#if defined(OS_CHROMEOS)
  // MediaCaptureDevicesDispatcher::Observer:
  void OnRequestUpdate(int render_process_id,
                       int render_frame_id,
                       blink::mojom::MediaStreamType stream_type,
                       const content::MediaRequestState state) override;

  // apps::AppWebContentsData::Client:
  void OnWebContentsDestroyed(content::WebContents* contents) override;
#endif

  // content_settings::Observer:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  void Init(bool observe_media_requests);

  apps::IconEffects GetIconEffects(const WebApp* web_app);

  const WebApp* GetWebApp(const AppId& app_id) const;

  // Returns the WebContents for the launch via `callback`. This value may be
  // null if the launch fails.
  void LaunchAppWithIntentImpl(
      const std::string& app_id,
      int32_t event_flags,
      apps::mojom::IntentPtr intent,
      apps::mojom::LaunchSource launch_source,
      int64_t display_id,
      base::OnceCallback<void(content::WebContents*)> callback);

#if defined(OS_CHROMEOS)
  // Updates app visibility.
  void UpdateAppDisabledMode(apps::mojom::AppPtr& app);

  bool MaybeAddNotification(const std::string& app_id,
                            const std::string& notification_id);
  void MaybeAddWebPageNotifications(
      const message_center::Notification& notification,
      const NotificationCommon::Metadata* const metadata);

  // Returns whether the app should show a badge.
  apps::mojom::OptionalBool ShouldShowBadge(
      const std::string& app_id,
      apps::mojom::OptionalBool has_notification_indicator);
#endif

  // Checks that the user permits the app launch (possibly presenting a blocking
  // user choice dialog). Launches the app with read access to the files in
  // `params.launch_files` and returns the created WebContents via `callback`,
  // or doesn't launch the app and returns null in `callback`.
  void LaunchAppWithFilesCheckingUserPermission(
      const std::string& app_id,
      apps::AppLaunchParams params,
      base::OnceCallback<void(content::WebContents*)> callback);

  // Called after the user has allowed or denied an app launch with files.
  void OnFileHandlerDialogCompleted(
      std::string app_id,
      apps::AppLaunchParams params,
      base::OnceCallback<void(content::WebContents*)> callback,
      bool allowed,
      bool remember_user_choice);

  const raw_ptr<Profile> profile_;

  const raw_ptr<WebAppProvider> provider_;

  // The app type of the publisher. The app type is kSystemWeb if the web apps
  // are serving from Lacros, and the app type is kWeb for all other cases.
  const apps::mojom::AppType app_type_;

  const raw_ptr<Delegate> delegate_;

  base::ScopedObservation<WebAppRegistrar, AppRegistrarObserver>
      registrar_observation_{this};

  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observation_{this};

  std::unique_ptr<WebAppLaunchManager> web_app_launch_manager_;

  bool is_shutting_down_ = false;

  apps_util::IncrementingIconKeyFactory icon_key_factory_;

  apps::PausedApps paused_apps_;

#if defined(OS_CHROMEOS)
  base::ScopedObservation<NotificationDisplayService,
                          NotificationDisplayService::Observer>
      notification_display_service_{this};

  apps::AppNotifications app_notifications_;

  badging::BadgeManager* badge_manager_ = nullptr;

  base::ScopedObservation<MediaCaptureDevicesDispatcher,
                          MediaCaptureDevicesDispatcher::Observer>
      media_dispatcher_{this};

  apps::MediaRequests media_requests_;
#endif

  std::map<std::string, WebAppShortcutsMenuItemInfo> shortcut_id_map_;
  ShortcutId::Generator shortcut_id_generator_;

  std::unique_ptr<web_app::LinkCapturingMigrationManager>
      link_capturing_migration_manager_;

  base::WeakPtrFactory<WebAppPublisherHelper> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APP_PUBLISHER_HELPER_H_
