// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/kiosk_vision.h"

#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/kiosk/vision/internal/pref_observer.h"
#include "chromeos/ash/components/kiosk/vision/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash::kiosk_vision {

namespace {

void InstallDlc(base::OnceCallback<void(std::string dlc_root_path)> on_done) {
  auto& dlc_service = CHECK_DEREF(DlcserviceClient::Get());
  dlcservice::InstallRequest install_request;
  install_request.set_id(kKioskVisionDlcId);
  dlc_service.Install(
      install_request,
      base::BindOnce(
          [](base::OnceCallback<void(std::string)> on_done,
             const DlcserviceClient::InstallResult& result) {
            std::move(on_done).Run(result.error == dlcservice::kErrorNone
                                       ? result.root_path
                                       : result.error);
          },
          std::move(on_done)),
      /*progress_callback=*/base::DoNothing());
}

void UninstallDlc() {
  auto& dlc_service = CHECK_DEREF(DlcserviceClient::Get());
  dlc_service.Uninstall(
      kKioskVisionDlcId, base::BindOnce([](const std::string& error) {
        if (error != dlcservice::kErrorNone) {
          LOG(WARNING) << "Failed to uninstall Kiosk Vision DLC: " << error;
        }
      }));
}

}  // namespace

KioskVision::KioskVision(PrefService* pref_service)
    : pref_observer_(
          pref_service,
          /*on_enabled=*/
          base::BindRepeating(&KioskVision::Enable, base::Unretained(this)),
          /*on_disabled=*/
          base::BindRepeating(&KioskVision::Disable, base::Unretained(this))) {
  if (!IsTelemetryPrefEnabled(CHECK_DEREF(pref_service))) {
    // Only uninstalls the DLC during construction, not during pref changes.
    // This avoids uninstalling the DLC while camera service is using it.
    UninstallDlc();
  }
}

KioskVision::~KioskVision() = default;

void KioskVision::Enable() {
  InstallDlc(base::BindOnce([](std::string dlc_path) {
    // TODO(b/320450634) Subscribe to CrOSCameraService detections.
  }));
}

void KioskVision::Disable() {
  // TODO(b/320450634) Unsubscribe to CrOSCameraService.
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kKioskVisionTelemetryEnabled,
                                /*default_value=*/false);
}

}  // namespace ash::kiosk_vision
