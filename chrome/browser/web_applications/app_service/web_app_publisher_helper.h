// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APP_PUBLISHER_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APP_PUBLISHER_HELPER_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/id_type.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/services/app_service/public/cpp/app.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/app_notifications.h"
#include "chrome/browser/apps/app_service/media_requests.h"
#include "chrome/browser/apps/app_service/paused_apps.h"
#include "chrome/browser/badging/badge_manager_delegate.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#endif

class Browser;
class ContentSettingsPattern;
class ContentSettingsTypeSet;
class Profile;
class GURL;

namespace apps {
struct AppLaunchParams;
enum class RunOnOsLoginMode;
enum IconEffects : uint32_t;
}  // namespace apps

namespace badging {
class BadgeManager;
}

namespace base {
class FilePath;
class Time;
}  // namespace base

namespace blink::mojom {
enum class DisplayMode : int32_t;
}

namespace content {
class WebContents;
}

#if BUILDFLAG(IS_CHROMEOS)
namespace message_center {
class Notification;
}
#endif

namespace ui {
enum ResourceScaleFactor : int;
}

namespace webapps {
enum class WebappUninstallSource;
}

namespace web_app {

class WebApp;
class WebAppProvider;
enum class RunOnOsLoginMode;
struct ComputedAppSize;

namespace mojom {
enum class UserDisplayMode : int32_t;
}

struct ShortcutIdTypeMarker {};

typedef base::IdTypeU32<ShortcutIdTypeMarker> ShortcutId;

void UninstallImpl(WebAppProvider* provider,
                   const std::string& app_id,
                   apps::UninstallSource uninstall_source,
                   gfx::NativeWindow parent_window);

class WebAppPublisherHelper : public WebAppRegistrarObserver,
                              public WebAppInstallManagerObserver,
#if BUILDFLAG(IS_CHROMEOS)
                              public NotificationDisplayService::Observer,
                              public MediaStreamCaptureIndicator::Observer,
#endif
                              public content_settings::Observer {
 public:
  class Delegate {
   public:
    Delegate();
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    ~Delegate();

    virtual void PublishWebApps(std::vector<apps::AppPtr> apps) = 0;
    virtual void PublishWebApp(apps::AppPtr app) = 0;

    virtual void ModifyWebAppCapabilityAccess(
        const std::string& app_id,
        std::optional<bool> accessing_camera,
        std::optional<bool> accessing_microphone) = 0;
  };

  using LoadIconCallback = base::OnceCallback<void(apps::IconValuePtr)>;

  WebAppPublisherHelper(Profile* profile,
                        WebAppProvider* provider,
                        Delegate* delegate);
  WebAppPublisherHelper(const WebAppPublisherHelper&) = delete;
  WebAppPublisherHelper& operator=(const WebAppPublisherHelper&) = delete;
  ~WebAppPublisherHelper() override;

  static apps::AppType GetWebAppType();

  // Indicates if |permission_type| is supported by Web Applications.
  static bool IsSupportedWebAppPermissionType(
      ContentSettingsType permission_type);

  // Must be called before profile keyed services are destroyed.
  void Shutdown();

  // Populates the various show_in_* fields of |app|.
  void SetWebAppShowInFields(const WebApp* web_app, apps::App& app);

  // Creates permissions for `web_app`.
  apps::Permissions CreatePermissions(const WebApp* web_app);

  // Creates an |apps::AppPtr| describing |web_app|.
  // Note: migration in progress. Changes should be made to both |CreateWebApp|
  // and |ConvertWebApp| until complete.
  apps::AppPtr CreateWebApp(const WebApp* web_app);

  // Constructs an App with only the information required to identify an
  // uninstallation.
  apps::AppPtr ConvertUninstalledWebApp(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source);

  // Constructs an App with only the information required to update
  // last launch time.
  apps::AppPtr ConvertLaunchedWebApp(const WebApp* web_app);

  // Directly uninstalls |web_app| without prompting the user.
  // If |clear_site_data| is true, any site data associated with the app will
  // be removed.
  // If |report_abuse| is true, the app will be reported for abuse to the Web
  // Store.
  void UninstallWebApp(const WebApp* web_app,
                       apps::UninstallSource uninstall_source,
                       bool clear_site_data,
                       bool report_abuse);

  void SetIconEffect(const std::string& app_id);

#if BUILDFLAG(IS_CHROMEOS)
  void PauseApp(const std::string& app_id);

  void UnpauseApp(const std::string& app_id);

  bool IsPaused(const std::string& app_id);

  void StopApp(const std::string& app_id);

  void GetCompressedIconData(const std::string& app_id,
                             int32_t size_in_dip,
                             ui::ResourceScaleFactor scale_factor,
                             apps::LoadIconCallback callback);
#endif  // BUILDFLAG(IS_CHROMEOS)

  void LoadIcon(const std::string& app_id,
                apps::IconType icon_type,
                int32_t size_hint_in_dip,
                apps::IconEffects icon_effects,
                apps::LoadIconCallback callback);

  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::LaunchSource launch_source,
              apps::WindowInfoPtr window_info,
              base::OnceCallback<void(content::WebContents*)> on_complete);

  void LaunchAppWithFiles(const std::string& app_id,
                          int32_t event_flags,
                          apps::LaunchSource launch_source,
                          std::vector<base::FilePath> file_paths);

  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           apps::IntentPtr intent,
                           apps::LaunchSource launch_source,
                           apps::WindowInfoPtr window_info,
                           apps::LaunchCallback callback);

  void LaunchAppWithParams(
      apps::AppLaunchParams params,
      base::OnceCallback<void(content::WebContents*)> on_complete);

  void SetPermission(const std::string& app_id, apps::PermissionPtr permission);

  void OpenNativeSettings(const std::string& app_id);

  apps::WindowMode GetWindowMode(const std::string& app_id);

  void UpdateAppSize(const std::string& app_id);

  void SetWindowMode(const std::string& app_id, apps::WindowMode window_mode);

  // Converts |display_mode| to a |window_mode|.
  apps::WindowMode ConvertDisplayModeToWindowMode(
      blink::mojom::DisplayMode display_mode);

  void PublishWindowModeUpdate(const std::string& app_id,
                               blink::mojom::DisplayMode display_mode);

  void PublishRunOnOsLoginModeUpdate(const std::string& app_id,
                                     RunOnOsLoginMode run_on_os_login_mode);

  std::string GenerateShortcutId();

  void StoreShortcutId(const std::string& shortcut_id,
                       const WebAppShortcutsMenuItemInfo& menu_item_info);

  // Execute the user command from the context menu items. Currently
  // on the web app shortcut need to be execute in the publisher.
  // The |app_id| represent the app that user selected, the |shortcut_id|
  // represents which shortcut item that user selected. The |display_id|
  // represent where to display the app.
  void ExecuteContextMenuCommand(
      const std::string& app_id,
      const std::string& shortcut_id,
      int64_t display_id,
      base::OnceCallback<void(content::WebContents*)> on_complete);

  // Checks that the user permits the app launch (possibly presenting a blocking
  // user choice dialog). Launches the app with read access to the files in
  // `params.launch_files` and returns all the created WebContentses via
  // `callback`, or doesn't launch the app and returns an empty vector in
  // `callback`.
  void LaunchAppWithFilesCheckingUserPermission(
      const std::string& app_id,
      apps::AppLaunchParams params,
      base::OnceCallback<void(std::vector<content::WebContents*>)> callback);

  Profile* profile() const { return profile_; }

  apps::AppType app_type() const { return app_type_; }

  WebAppRegistrar& registrar() const;
  WebAppInstallManager& install_manager() const;

  bool IsShuttingDown() const;

  static apps::IntentFilters CreateIntentFiltersForWebApp(
      const WebAppProvider& provider,
      const web_app::WebApp& app);

 private:
#if BUILDFLAG(IS_CHROMEOS)
  class BadgeManagerDelegate : public badging::BadgeManagerDelegate {
   public:
    explicit BadgeManagerDelegate(
        const base::WeakPtr<WebAppPublisherHelper>& publisher_helper);

    ~BadgeManagerDelegate() override;

    void OnAppBadgeUpdated(const webapps::AppId& app_id) override;

   private:
    base::WeakPtr<WebAppPublisherHelper> publisher_helper_;
  };
#endif  // BUILDFLAG(IS_CHROMEOS)

  // WebAppInstallManagerObserver:
  void OnWebAppInstalled(const webapps::AppId& app_id) override;
  void OnWebAppInstalledWithOsHooks(const webapps::AppId& app_id) override;
  void OnWebAppManifestUpdated(const webapps::AppId& app_id) override;
  void OnWebAppUninstalled(
      const webapps::AppId& app_id,
      webapps::WebappUninstallSource uninstall_source) override;
  void OnWebAppInstallManagerDestroyed() override;

  // WebAppRegistrarObserver:
  void OnAppRegistrarDestroyed() override;
  void OnWebAppFileHandlerApprovalStateChanged(
      const webapps::AppId& app_id) override;
  void OnWebAppLastLaunchTimeChanged(
      const std::string& app_id,
      const base::Time& last_launch_time) override;
  void OnWebAppUserDisplayModeChanged(
      const webapps::AppId& app_id,
      mojom::UserDisplayMode user_display_mode) override;
  void OnWebAppRunOnOsLoginModeChanged(
      const webapps::AppId& app_id,
      RunOnOsLoginMode run_on_os_login_mode) override;
  void OnWebAppSettingsPolicyChanged() override;

#if BUILDFLAG(IS_CHROMEOS)
  void OnWebAppDisabledStateChanged(const webapps::AppId& app_id,
                                    bool is_disabled) override;
  void OnWebAppsDisabledModeChanged() override;

  // NotificationDisplayService::Observer overrides.
  void OnNotificationDisplayed(
      const message_center::Notification& notification,
      const NotificationCommon::Metadata* metadata) override;
  void OnNotificationClosed(const std::string& notification_id) override;
  void OnNotificationDisplayServiceDestroyed(
      NotificationDisplayService* service) override;
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
  // MediaStreamCaptureIndicator::Observer:
  void OnIsCapturingVideoChanged(content::WebContents* web_contents,
                                 bool is_capturing_video) override;
  void OnIsCapturingAudioChanged(content::WebContents* web_contents,
                                 bool is_capturing_audio) override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // content_settings::Observer:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  void Init();

  void ObserveWebAppSubsystems();

  apps::IconEffects GetIconEffects(const WebApp* web_app);

  const WebApp* GetWebApp(const webapps::AppId& app_id) const;

  // Returns all the WebContents instances launched via `callback`. This value
  // may be empty if the launch fails. There may be more than one `WebContents`
  // if each file is launched in a different window.
  void LaunchAppWithIntentImpl(
      const std::string& app_id,
      int32_t event_flags,
      apps::IntentPtr intent,
      apps::LaunchSource launch_source,
      int64_t display_id,
      base::OnceCallback<void(std::vector<content::WebContents*>)> callback);

  // Get the list of identifiers for the app that will be used in policy
  // controls, such as force-installation and pinning. May be empty.
  std::vector<std::string> GetPolicyIds(const WebApp& web_app) const;

  apps::PackageId GetPackageId(const WebApp& web_app) const;

#if BUILDFLAG(IS_CHROMEOS)
  // Updates app visibility.
  void UpdateAppDisabledMode(apps::App& app);

  bool MaybeAddNotification(const std::string& app_id,
                            const std::string& notification_id);
  void MaybeAddWebPageNotifications(
      const message_center::Notification& notification,
      const NotificationCommon::Metadata* metadata);

  // Returns whether the app should show a badge.
  bool ShouldShowBadge(const std::string& app_id,
                       bool has_notification_indicator);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Called after the user has allowed or denied an app launch with files.
  void OnFileHandlerDialogCompleted(
      std::string app_id,
      apps::AppLaunchParams params,
      base::OnceCallback<void(std::vector<content::WebContents*>)> callback,
      bool allowed,
      bool remember_user_choice);

  void OnLaunchCompleted(
      apps::AppLaunchParams params_for_restore,
      bool is_system_web_app,
      std::optional<GURL> override_url,
      base::OnceCallback<void(content::WebContents*)> on_complete,
      base::WeakPtr<Browser> browser,
      base::WeakPtr<content::WebContents> web_contents,
      apps::LaunchContainer container);

  void OnGetWebAppSize(webapps::AppId app_id,
                       std::optional<ComputedAppSize> size);

  const raw_ptr<Profile, DanglingUntriaged> profile_;

  const raw_ptr<WebAppProvider, DanglingUntriaged> provider_;

  // The app type of the publisher. The app type is kSystemWeb if the web apps
  // are serving from Lacros, and the app type is kWeb for all other cases.
  const apps::AppType app_type_;

  const raw_ptr<Delegate, DanglingUntriaged> delegate_;

  base::ScopedObservation<WebAppRegistrar, WebAppRegistrarObserver>
      registrar_observation_{this};

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};

  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observation_{this};

  bool is_shutting_down_ = false;

#if BUILDFLAG(IS_CHROMEOS)
  apps::PausedApps paused_apps_;

  base::ScopedObservation<NotificationDisplayService,
                          NotificationDisplayService::Observer>
      notification_display_service_{this};

  apps::AppNotifications app_notifications_;

  raw_ptr<badging::BadgeManager, DanglingUntriaged> badge_manager_ = nullptr;

  base::ScopedObservation<MediaStreamCaptureIndicator,
                          MediaStreamCaptureIndicator::Observer>
      media_indicator_observation_{this};

  apps::MediaRequests media_requests_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  std::map<std::string, WebAppShortcutsMenuItemInfo> shortcut_id_map_;
  ShortcutId::Generator shortcut_id_generator_;

  base::WeakPtrFactory<WebAppPublisherHelper> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APP_PUBLISHER_HELPER_H_
