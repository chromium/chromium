// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

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
//
// TODO(crbug.com/1253250):
// 1. Remove the parent class apps::PublisherBase.
// 2. Remove all apps::mojom related code.
class WebApps : public apps::PublisherBase,
                public apps::AppPublisher,
                public WebAppPublisherHelper::Delegate,
                public base::SupportsWeakPtr<WebApps> {
 public:
  explicit WebApps(apps::AppServiceProxy* proxy);
  WebApps(const WebApps&) = delete;
  WebApps& operator=(const WebApps&) = delete;
  ~WebApps() override;

  virtual void Shutdown();

 protected:
  const WebApp* GetWebApp(const AppId& app_id) const;

  const mojo::RemoteSet<apps::mojom::Subscriber>& subscribers() const {
    return subscribers_;
  }

  Profile* profile() const { return profile_; }
  WebAppProvider* provider() const { return provider_; }

  apps::AppType app_type() { return app_type_; }

  WebAppPublisherHelper& publisher_helper() { return publisher_helper_; }

 private:
  void Initialize(const mojo::Remote<apps::mojom::AppService>& app_service);

  // apps::AppPublisher overrides.
  void LoadIcon(const std::string& app_id,
                const apps::IconKey& icon_key,
                apps::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                apps::LoadIconCallback callback) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::LaunchSource launch_source,
              apps::WindowInfoPtr window_info) override;
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           apps::IntentPtr intent,
                           apps::LaunchSource launch_source,
                           apps::WindowInfoPtr window_info,
                           base::OnceCallback<void(bool)> callback);
  void LaunchAppWithParams(apps::AppLaunchParams&& params,
                           apps::LaunchCallback callback) override;
  void LaunchShortcut(const std::string& app_id,
                      const std::string& shortcut_id,
                      int64_t display_id) override;

  // apps::mojom::Publisher overrides.
  void Connect(mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
               apps::mojom::ConnectOptionsPtr opts) override;
  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              apps::mojom::WindowInfoPtr window_info) override;
  void LaunchAppWithFiles(const std::string& app_id,
                          int32_t event_flags,
                          apps::mojom::LaunchSource launch_source,
                          apps::mojom::FilePathsPtr file_paths) override;
  void LaunchAppWithIntent(const std::string& app_id,
                           int32_t event_flags,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           apps::mojom::WindowInfoPtr window_info,
                           LaunchAppWithIntentCallback callback) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void SetWindowMode(const std::string& app_id,
                     apps::mojom::WindowMode window_mode) override;
  void SetRunOnOsLoginMode(
      const std::string& app_id,
      apps::mojom::RunOnOsLoginMode run_on_os_login_mode) override;

  // WebAppPublisherHelper::Delegate overrides.
  void PublishWebApps(std::vector<apps::AppPtr> apps) override;
  void PublishWebApp(apps::AppPtr app) override;
  void ModifyWebAppCapabilityAccess(
      const std::string& app_id,
      absl::optional<bool> accessing_camera,
      absl::optional<bool> accessing_microphone) override;

  std::vector<apps::AppPtr> CreateWebApps();
  void ConvertWebApps(std::vector<apps::mojom::AppPtr>* apps_out);
  void InitWebApps();
  void StartPublishingWebApps(
      mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // apps::mojom::Publisher overrides.
  void Uninstall(const std::string& app_id,
                 apps::mojom::UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void PauseApp(const std::string& app_id) override;
  void UnpauseApp(const std::string& app_id) override;
  void StopApp(const std::string& app_id) override;
  void GetMenuModel(const std::string& app_id,
                    apps::mojom::MenuType menu_type,
                    int64_t display_id,
                    GetMenuModelCallback callback) override;
  // menu_type is stored as |shortcut_id|.
  void ExecuteContextMenuCommand(const std::string& app_id,
                                 int command_id,
                                 const std::string& shortcut_id,
                                 int64_t display_id) override;

  void GetAppShortcutMenuModel(const std::string& app_id,
                               apps::mojom::MenuItemsPtr menu_items,
                               GetMenuModelCallback callback);

  void OnShortcutsMenuIconsRead(
      const std::string& app_id,
      apps::mojom::MenuItemsPtr menu_items,
      GetMenuModelCallback callback,
      ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  const raw_ptr<Profile> profile_;

  const raw_ptr<WebAppProvider> provider_;

  // app_service_ is owned by the object that owns this object.
  raw_ptr<apps::mojom::AppService> app_service_;

  // The app type of the publisher. The app type is kSystemWeb if the web apps
  // are serving from Lacros, and the app type is kWeb for all other cases.
  const apps::AppType app_type_;

  // Specifies whether the web app registry becomes ready.
  bool is_ready_ = false;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  apps::InstanceRegistry* const instance_registry_;
#endif

  WebAppPublisherHelper publisher_helper_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_H_
