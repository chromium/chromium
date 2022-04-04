// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_APP_SERVICE_MOJOM_IMPL_H_
#define COMPONENTS_SERVICES_APP_SERVICE_APP_SERVICE_MOJOM_IMPL_H_

#include <map>

#include "base/files/file_path.h"
#include "components/services/app_service/public/cpp/preferred_apps.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace apps {

class PreferredAppsList;

// The implementation of the apps::mojom::AppService Mojo interface.
//
// See components/services/app_service/README.md.
class AppServiceMojomImpl : public apps::mojom::AppService,
                            public PreferredApps::Host {
 public:
  AppServiceMojomImpl(
      const base::FilePath& profile_dir,
      base::OnceClosure read_completed_for_testing = base::OnceClosure(),
      base::OnceClosure write_completed_for_testing = base::OnceClosure());

  AppServiceMojomImpl(const AppServiceMojomImpl&) = delete;
  AppServiceMojomImpl& operator=(const AppServiceMojomImpl&) = delete;

  ~AppServiceMojomImpl() override;

  void BindReceiver(mojo::PendingReceiver<apps::mojom::AppService> receiver);

  void FlushMojoCallsForTesting();

  // apps::mojom::AppService overrides.
  void RegisterPublisher(
      mojo::PendingRemote<apps::mojom::Publisher> publisher_remote,
      apps::mojom::AppType app_type) override;
  void RegisterSubscriber(
      mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
      apps::mojom::ConnectOptionsPtr opts) override;
  void LoadIcon(apps::mojom::AppType app_type,
                const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override;
  void Launch(apps::mojom::AppType app_type,
              const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              apps::mojom::WindowInfoPtr window_info) override;
  void LaunchAppWithFiles(apps::mojom::AppType app_type,
                          const std::string& app_id,
                          int32_t event_flags,
                          apps::mojom::LaunchSource launch_source,
                          apps::mojom::FilePathsPtr file_paths) override;
  void LaunchAppWithIntent(apps::mojom::AppType app_type,
                           const std::string& app_id,
                           int32_t event_flags,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           apps::mojom::WindowInfoPtr window_info,
                           LaunchAppWithIntentCallback callback) override;
  void SetPermission(apps::mojom::AppType app_type,
                     const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void Uninstall(apps::mojom::AppType app_type,
                 const std::string& app_id,
                 apps::mojom::UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void PauseApp(apps::mojom::AppType app_type,
                const std::string& app_id) override;
  void UnpauseApp(apps::mojom::AppType app_type,
                  const std::string& app_id) override;
  void StopApp(apps::mojom::AppType app_type,
               const std::string& app_id) override;
  void GetMenuModel(apps::mojom::AppType app_type,
                    const std::string& app_id,
                    apps::mojom::MenuType menu_type,
                    int64_t display_id,
                    GetMenuModelCallback callback) override;
  void ExecuteContextMenuCommand(apps::mojom::AppType app_type,
                                 const std::string& app_id,
                                 int command_id,
                                 const std::string& shortcut_id,
                                 int64_t display_id) override;
  void OpenNativeSettings(apps::mojom::AppType app_type,
                          const std::string& app_id) override;
  void AddPreferredApp(apps::mojom::AppType app_type,
                       const std::string& app_id,
                       apps::mojom::IntentFilterPtr intent_filter,
                       apps::mojom::IntentPtr intent,
                       bool from_publisher) override;
  void RemovePreferredApp(apps::mojom::AppType app_type,
                          const std::string& app_id) override;
  void RemovePreferredAppForFilter(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      apps::mojom::IntentFilterPtr intent_filter) override;
  void SetSupportedLinksPreference(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      std::vector<apps::mojom::IntentFilterPtr> all_link_filters) override;
  void RemoveSupportedLinksPreference(apps::mojom::AppType app_type,
                                      const std::string& app_id) override;
  void SetResizeLocked(apps::mojom::AppType app_type,
                       const std::string& app_id,
                       apps::mojom::OptionalBool locked) override;
  void SetWindowMode(apps::mojom::AppType app_type,
                     const std::string& app_id,
                     apps::mojom::WindowMode window_mode) override;
  void SetRunOnOsLoginMode(
      apps::mojom::AppType app_type,
      const std::string& app_id,
      apps::mojom::RunOnOsLoginMode run_on_os_login_mode) override;

  // PreferredApps::Host overrides.
  void InitializePreferredAppsForAllSubscribers() override;

  void OnPreferredAppsChanged(
      apps::mojom::PreferredAppChangesPtr changes) override;

  void OnPreferredAppSet(
      const std::string& app_id,
      apps::mojom::IntentFilterPtr intent_filter,
      apps::mojom::IntentPtr intent,
      apps::mojom::ReplacedAppPreferencesPtr replaced_app_preferences) override;

  void OnSupportedLinksPreferenceChanged(const std::string& app_id,
                                         bool open_in_app) override;

  // Returns publisher for `app_type`, or nullptr if there is no publisher for
  // `app_type`.
  apps::mojom::Publisher* GetPublisher(apps::mojom::AppType app_type) override;

  // Retern the preferred_apps_list_ for testing.
  PreferredAppsList& GetPreferredAppsListForTesting();

 private:
  void OnPublisherDisconnected(apps::mojom::AppType app_type);

  // publishers_ is a std::map, not a mojo::RemoteSet, since we want to
  // be able to find *the* publisher for a given apps::mojom::AppType.
  std::map<apps::mojom::AppType, mojo::Remote<apps::mojom::Publisher>>
      publishers_;
  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  // Must come after the publisher and subscriber maps to ensure it is
  // destroyed first, closing the connection to avoid dangling callbacks.
  mojo::ReceiverSet<apps::mojom::AppService> receivers_;

  PreferredApps preferred_apps_;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_APP_SERVICE_MOJOM_IMPL_H_
