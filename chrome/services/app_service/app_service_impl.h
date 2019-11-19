// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_APP_SERVICE_IMPL_H_
#define CHROME_SERVICES_APP_SERVICE_APP_SERVICE_IMPL_H_

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/services/app_service/public/cpp/preferred_apps.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class PrefService;

namespace service_manager {
class Connector;
}

namespace apps {

// The implementation of the apps::mojom::AppService Mojo interface.
//
// See chrome/services/app_service/README.md.
class AppServiceImpl : public apps::mojom::AppService {
 public:
  explicit AppServiceImpl(service_manager::Connector* connector);
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
                apps::mojom::IconCompression icon_compression,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                LoadIconCallback callback) override;
  void Launch(apps::mojom::AppType app_type,
              const std::string& app_id,
              int32_t event_flags,
              apps::mojom::LaunchSource launch_source,
              int64_t display_id) override;
  void LaunchAppWithIntent(apps::mojom::AppType app_type,
                           const std::string& app_id,
                           apps::mojom::IntentPtr intent,
                           apps::mojom::LaunchSource launch_source,
                           int64_t display_id) override;
  void SetPermission(apps::mojom::AppType app_type,
                     const std::string& app_id,
                     apps::mojom::PermissionPtr permission) override;
  void PromptUninstall(apps::mojom::AppType app_type,
                       const std::string& app_id) override;
  void Uninstall(apps::mojom::AppType app_type,
                 const std::string& app_id,
                 bool clear_site_data,
                 bool report_abuse) override;
  void PauseApp(apps::mojom::AppType app_type,
                const std::string& app_id) override;
  void UnpauseApps(apps::mojom::AppType app_type,
                   const std::string& app_id) override;
  void OpenNativeSettings(apps::mojom::AppType app_type,
                          const std::string& app_id) override;
  void AddPreferredApp(apps::mojom::AppType app_type,
                       const std::string& app_id,
                       apps::mojom::IntentFilterPtr intent_filter,
                       apps::mojom::IntentPtr intent) override;
  void RemovePreferredApp(apps::mojom::AppType app_type,
                          const std::string& app_id) override;

  // Retern the preferred_apps_ for testing.
  PreferredApps& GetPreferredAppsForTesting();

 private:
  void OnPublisherDisconnected(apps::mojom::AppType app_type);

  // Initialize the preferred apps from disk.
  void InitializePreferredApps();

  void ConnectToPrefService(service_manager::Connector* connector);

  void OnPrefServiceConnected(std::unique_ptr<PrefService> pref_service);

  // publishers_ is a std::map, not a mojo::RemoteSet, since we want to
  // be able to find *the* publisher for a given apps::mojom::AppType.
  std::map<apps::mojom::AppType, mojo::Remote<apps::mojom::Publisher>>
      publishers_;
  mojo::RemoteSet<apps::mojom::Subscriber> subscribers_;

  // Must come after the publisher and subscriber maps to ensure it is
  // destroyed first, closing the connection to avoid dangling callbacks.
  mojo::ReceiverSet<apps::mojom::AppService> receivers_;

  std::unique_ptr<PrefService> pref_service_;

  PreferredApps preferred_apps_;

  base::WeakPtrFactory<AppServiceImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppServiceImpl);
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_APP_SERVICE_IMPL_H_
