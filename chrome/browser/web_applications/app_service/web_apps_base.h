// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_BASE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "components/services/app_service/public/cpp/publisher_base.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace webapps {
enum class WebappUninstallSource;
}  // namespace webapps

namespace web_app {

class WebApp;
class WebAppProvider;
class WebAppRegistrar;

// An app publisher (in the App Service sense) of Web Apps.
class WebAppsBase : public apps::PublisherBase,
                    public WebAppPublisherHelper::Delegate,
                    public base::SupportsWeakPtr<WebAppsBase> {
 public:
  WebAppsBase(const mojo::Remote<apps::mojom::AppService>& app_service,
              Profile* profile);
  WebAppsBase(const WebAppsBase&) = delete;
  WebAppsBase& operator=(const WebAppsBase&) = delete;
  ~WebAppsBase() override;

  virtual void Shutdown();

 protected:
  const WebApp* GetWebApp(const AppId& app_id) const;

  bool Accepts(const std::string& app_id) const;

  const mojo::RemoteSet<apps::mojom::Subscriber>& subscribers() const {
    return subscribers_;
  }

  Profile* profile() const { return profile_; }
  WebAppProvider* provider() const { return provider_; }

  // Can return nullptr in tests.
  const WebAppRegistrar* GetRegistrar() const;

  apps::mojom::AppType app_type() { return app_type_; }

  WebAppPublisherHelper& publisher_helper() { return publisher_helper_; }

 private:
  void Initialize(const mojo::Remote<apps::mojom::AppService>& app_service);

  // apps::mojom::Publisher overrides.
  void Connect(mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
               apps::mojom::ConnectOptionsPtr opts) override;
  void LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override;
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
                           apps::mojom::WindowInfoPtr window_info) override;
  void SetPermission(const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void OpenNativeSettings(const std::string& app_id) override;

  // WebAppPublisherHelper::Delegate overrides.
  void PublishWebApps(std::vector<apps::mojom::AppPtr> apps) override;
  void PublishWebApp(apps::mojom::AppPtr app) override;
  void ModifyWebAppCapabilityAccess(
      const std::string& app_id,
      absl::optional<bool> accessing_camera,
      absl::optional<bool> accessing_microphone) override;

  void ConvertWebApps(std::vector<apps::mojom::AppPtr>* apps_out);
  void StartPublishingWebApps(
      mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote);

  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  Profile* const profile_;

  WebAppProvider* provider_ = nullptr;

  // app_service_ is owned by the object that owns this object.
  apps::mojom::AppService* app_service_;

  // The app type of the publisher. The app type is kSystemWeb if the web apps
  // are serving from Lacros, and the app type is kWeb for all other cases.
  apps::mojom::AppType app_type_;

  WebAppPublisherHelper publisher_helper_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_WEB_APPS_BASE_H_
