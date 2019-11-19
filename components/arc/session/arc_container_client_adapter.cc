// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_container_client_adapter.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/login_manager/arc.pb.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "components/arc/session/arc_session.h"

namespace arc {
namespace {

// Converts PackageCacheMode into login_manager's.
login_manager::UpgradeArcContainerRequest_PackageCacheMode
ToLoginManagerPackageCacheMode(UpgradeParams::PackageCacheMode mode) {
  switch (mode) {
    case UpgradeParams::PackageCacheMode::DEFAULT:
      return login_manager::UpgradeArcContainerRequest_PackageCacheMode_DEFAULT;
    case UpgradeParams::PackageCacheMode::COPY_ON_INIT:
      return login_manager::
          UpgradeArcContainerRequest_PackageCacheMode_COPY_ON_INIT;
    case UpgradeParams::PackageCacheMode::SKIP_SETUP_COPY_ON_INIT:
      return login_manager::
          UpgradeArcContainerRequest_PackageCacheMode_SKIP_SETUP_COPY_ON_INIT;
  }
}

// Converts ArcSupervisionTransition into login_manager's.
login_manager::UpgradeArcContainerRequest_SupervisionTransition
ToLoginManagerSupervisionTransition(ArcSupervisionTransition transition) {
  switch (transition) {
    case ArcSupervisionTransition::NO_TRANSITION:
      return login_manager::
          UpgradeArcContainerRequest_SupervisionTransition_NONE;
    case ArcSupervisionTransition::CHILD_TO_REGULAR:
      return login_manager::
          UpgradeArcContainerRequest_SupervisionTransition_CHILD_TO_REGULAR;
    case ArcSupervisionTransition::REGULAR_TO_CHILD:
      return login_manager::
          UpgradeArcContainerRequest_SupervisionTransition_REGULAR_TO_CHILD;
  }
}

// Converts PlayStoreAutoUpdate into login_manager's.
login_manager::StartArcMiniContainerRequest_PlayStoreAutoUpdate
ToLoginManagerPlayStoreAutoUpdate(StartParams::PlayStoreAutoUpdate update) {
  switch (update) {
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_DEFAULT:
      return login_manager::
          StartArcMiniContainerRequest_PlayStoreAutoUpdate_AUTO_UPDATE_DEFAULT;
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_ON:
      return login_manager::
          StartArcMiniContainerRequest_PlayStoreAutoUpdate_AUTO_UPDATE_ON;
    case StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_OFF:
      return login_manager::
          StartArcMiniContainerRequest_PlayStoreAutoUpdate_AUTO_UPDATE_OFF;
  }
}

}  // namespace

class ArcContainerClientAdapter
    : public ArcClientAdapter,
      public chromeos::SessionManagerClient::Observer,
      public chromeos::UpstartClient::Observer {
 public:
  ArcContainerClientAdapter() {
    if (chromeos::SessionManagerClient::Get())
      chromeos::SessionManagerClient::Get()->AddObserver(this);
    if (chromeos::UpstartClient::Get())
      chromeos::UpstartClient::Get()->AddObserver(this);
  }

  ~ArcContainerClientAdapter() override {
    if (chromeos::SessionManagerClient::Get())
      chromeos::SessionManagerClient::Get()->RemoveObserver(this);
    if (chromeos::UpstartClient::Get())
      chromeos::UpstartClient::Get()->RemoveObserver(this);
  }

  // ArcClientAdapter overrides:
  void StartMiniArc(StartParams params,
                    chromeos::VoidDBusMethodCallback callback) override {
    login_manager::StartArcMiniContainerRequest request;
    request.set_native_bridge_experiment(params.native_bridge_experiment);
    request.set_lcd_density(params.lcd_density);
    request.set_arc_file_picker_experiment(params.arc_file_picker_experiment);
    request.set_play_store_auto_update(
        ToLoginManagerPlayStoreAutoUpdate(params.play_store_auto_update));
    request.set_arc_custom_tabs_experiment(params.arc_custom_tabs_experiment);
    request.set_arc_print_spooler_experiment(
        params.arc_print_spooler_experiment);

    chromeos::SessionManagerClient::Get()->StartArcMiniContainer(
        request, std::move(callback));
  }

  void UpgradeArc(UpgradeParams params,
                  chromeos::VoidDBusMethodCallback callback) override {
    login_manager::UpgradeArcContainerRequest request;
    request.set_account_id(params.account_id);
    request.set_is_account_managed(params.is_account_managed);
    request.set_skip_boot_completed_broadcast(
        params.skip_boot_completed_broadcast);
    request.set_packages_cache_mode(
        ToLoginManagerPackageCacheMode(params.packages_cache_mode));
    request.set_skip_gms_core_cache(params.skip_gms_core_cache);
    request.set_is_demo_session(params.is_demo_session);
    request.set_demo_session_apps_path(params.demo_session_apps_path.value());
    request.set_locale(params.locale);
    for (const auto& language : params.preferred_languages)
      request.add_preferred_languages(language);
    request.set_supervision_transition(
        ToLoginManagerSupervisionTransition(params.supervision_transition));

    chromeos::SessionManagerClient::Get()->UpgradeArcContainer(
        request, std::move(callback));
  }

  void StopArcInstance(bool) override {
    // Since we have the ArcInstanceStopped() callback, we don't need to do
    // anything when StopArcInstance completes.
    chromeos::SessionManagerClient::Get()->StopArcInstance(
        chromeos::EmptyVoidDBusMethodCallback());
  }

  void SetUserInfo(const std::string& hash,
                   const std::string& serial_number) override {}

  // chromeos::SessionManagerClient::Observer overrides:
  void ArcInstanceStopped() override {
    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped();
  }

  // chromeos::UpstartClient::Observer overrides:
  void ArcStopped() override {
    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcContainerClientAdapter);
};

std::unique_ptr<ArcClientAdapter> CreateArcContainerClientAdapter() {
  return std::make_unique<ArcContainerClientAdapter>();
}

}  // namespace arc
