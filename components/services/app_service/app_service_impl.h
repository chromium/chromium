// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_APP_SERVICE_IMPL_H_
#define COMPONENTS_SERVICES_APP_SERVICE_APP_SERVICE_IMPL_H_

#include <map>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/services/app_service/public/cpp/preferred_apps_list.h"
#include "components/services/app_service/public/mojom/app_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace apps {

// The implementation of the apps::mojom::AppService Mojo interface.
//
// See components/services/app_service/README.md.
class AppServiceImpl : public apps::mojom::AppService {
 public:
  AppServiceImpl(
      const base::FilePath& profile_dir,
      bool is_share_intents_supported,
      base::OnceClosure read_completed_for_testing = base::OnceClosure(),
      base::OnceClosure write_completed_for_testing = base::OnceClosure());
  ~AppServiceImpl() override;

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
              int64_t display_id) override;
  void LaunchAppWithFiles(apps::mojom::AppType app_type,
                          const std::string& app_id,
                          apps::mojom::LaunchContainer container,
                          int32_t event_flags,
                          apps::mojom::LaunchSource launch_source,
                          apps::mojom::FilePathsPtr file_paths) override;
  void LaunchAppWithIntent(apps::mojom::AppType app_type,
                           const std::string& app_id,
                           int32_t event_flags,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           int64_t display_id) override;
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
  void UnpauseApps(apps::mojom::AppType app_type,
                   const std::string& app_id) override;
  void StopApp(apps::mojom::AppType app_type,
               const std::string& app_id) override;
  void GetMenuModel(apps::mojom::AppType app_type,
                    const std::string& app_id,
                    apps::mojom::MenuType menu_type,
                    int64_t display_id,
                    GetMenuModelCallback callback) override;
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

  // Retern the preferred_apps_ for testing.
  PreferredAppsList& GetPreferredAppsForTesting();

  void SetWriteCompletedCallbackForTesting(base::OnceClosure testing_callback);

 private:
  void OnPublisherDisconnected(apps::mojom::AppType app_type);

  // Initialize the preferred apps from disk.
  void InitializePreferredApps();

  // Write the preferred apps to a json file.
  void WriteToJSON(const base::FilePath& profile_dir,
                   const apps::PreferredAppsList& preferred_apps);

  void WriteCompleted();

  void ReadFromJSON(const base::FilePath& profile_dir);

  void ReadCompleted(std::string preferred_apps_string);

  // publishers_ is a std::map, not a mojo::RemoteSet, since we want to
  // be able to find *the* publisher for a given apps::mojom::AppType.
  std::map<apps::mojom::AppType, mojo::Remote<apps::mojom::Publisher>>
      publishers_;
  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  // Must come after the publisher and subscriber maps to ensure it is
  // destroyed first, closing the connection to avoid dangling callbacks.
  mojo::ReceiverSet<apps::mojom::AppService> receivers_;

  PreferredAppsList preferred_apps_;

  base::FilePath profile_dir_;

  // True if the kIntentHandlingSharing flag is on. This is used to see if
  // we need to convert the stored preferred app to the new version that
  // supports sharing.
  // TODO(crbug.com/1092784): remove when the kIntentHandlingSharing flag is
  // removed.
  bool is_share_intents_supported_;

  // True if need to write preferred apps to file after the current write is
  // completed.
  bool should_write_preferred_apps_to_file_;

  // True if it is currently writing preferred apps to file.
  bool writing_preferred_apps_;

  // Task runner where the file operations takes place. This is to make sure the
  // write operation will be operated in sequence.
  scoped_refptr<base::SequencedTaskRunner> const task_runner_;

  base::OnceClosure read_completed_for_testing_;

  base::OnceClosure write_completed_for_testing_;

  base::WeakPtrFactory<AppServiceImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppServiceImpl);
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_APP_SERVICE_IMPL_H_
