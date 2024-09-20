// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/menu.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#endif

static_assert(!BUILDFLAG(IS_CHROMEOS_LACROS), "For non-Lacros only");

class Profile;

namespace webapps {
enum class WebappUninstallSource;
}  // namespace webapps

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace apps {
class InstanceRegistry;
struct AppLaunchParams;
}
#endif

namespace web_app {

class WebApp;
class WebAppProvider;

// An app publisher (in the App Service sense) of Web Apps.
class WebApps final : public apps::AppPublisher,
                      public WebAppPublisherHelper::Delegate {
 public:
  explicit WebApps(apps::AppServiceProxy* proxy);
  WebApps(const WebApps&) = delete;
  WebApps& operator=(const WebApps&) = delete;
  ~WebApps() override;

  virtual void Shutdown();

 protected:
  const WebApp* GetWebApp(const webapps::AppId& app_id) const;

  Profile* profile() const { return profile_; }
  WebAppProvider* provider() const { return provider_; }

  apps::AppType app_type() { return publisher_helper_.app_type(); }

  WebAppPublisherHelper& publisher_helper() { return publisher_helper_; }

 private:
  void Initialize();

  // apps::AppPublisher overrides.
  void LoadIcon(const std::string& app_id,
                const apps::IconKey& icon_key,
                apps::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                apps::LoadIconCallback callback) override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void GetCompressedIconData(const std::string& app_id,
                             int32_t size_in_dip,
                             ui::ResourceScaleFactor scale_factor,
                             apps::LoadIconCallback callback) override;
#endif
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::LaunchSource launch_source,
              apps::WindowInfoPtr window_info) override;
  void LaunchAppWithFiles(const std::string& app_id,
                          int32_t event_flags,
                          apps::LaunchSource launch_source,
                          std::vector<base::FilePath> file_paths) override;
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           apps::IntentPtr intent,
                           apps::LaunchSource launch_source,
                           apps::WindowInfoPtr window_info,
                           apps::LaunchCallback callback) override;
  void LaunchAppWithParams(apps::AppLaunchParams&& params,
                           apps::LaunchCallback callback) override;
  void LaunchShortcut(const std::string& app_id,
                      const std::string& shortcut_id,
                      int64_t display_id) override;
  void SetPermission(const std::string& app_id,
                     apps::PermissionPtr permission) override;
  void Uninstall(const std::string& app_id,
                 apps::UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void GetMenuModel(
      const std::string& app_id,
      apps::MenuType menu_type,
      int64_t display_id,
      base::OnceCallback<void(apps::MenuItems)> callback) override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void UpdateAppSize(const std::string& app_id) override;

  void SetWindowMode(const std::string& app_id,
                     apps::WindowMode window_mode) override;

  void OpenNativeSettings(const std::string& app_id) override;

  // WebAppPublisherHelper::Delegate overrides.
  void PublishWebApps(std::vector<apps::AppPtr> apps) override;
  void PublishWebApp(apps::AppPtr app) override;
  void ModifyWebAppCapabilityAccess(
      const std::string& app_id,
      std::optional<bool> accessing_camera,
      std::optional<bool> accessing_microphone) override;

  std::vector<apps::AppPtr> CreateWebApps();
  void InitWebApps();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // apps::AppPublisher overrides.
  void PauseApp(const std::string& app_id) override;
  void UnpauseApp(const std::string& app_id) override;
  void StopApp(const std::string& app_id) override;
  // menu_type is stored as |shortcut_id|.
  void ExecuteContextMenuCommand(const std::string& app_id,
                                 int command_id,
                                 const std::string& shortcut_id,
                                 int64_t display_id) override;

  void GetAppShortcutMenuModel(
      const std::string& app_id,
      apps::MenuItems menu_items,
      base::OnceCallback<void(apps::MenuItems)> callback);

  void OnShortcutsMenuIconsRead(
      const std::string& app_id,
      apps::MenuItems menu_items,
      base::OnceCallback<void(apps::MenuItems)> callback,
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps);

  void LaunchMallWithContext(int32_t event_flags,
                             apps::LaunchSource launch_source,
                             apps::WindowInfoPtr window_info,
                             apps::DeviceInfo device_info);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  const raw_ptr<Profile> profile_;

  const raw_ptr<WebAppProvider> provider_;

  // Specifies whether the web app registry becomes ready.
  bool is_ready_ = false;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const raw_ptr<apps::InstanceRegistry> instance_registry_;
#endif

  WebAppPublisherHelper publisher_helper_;
  base::WeakPtrFactory<WebApps> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_H_
