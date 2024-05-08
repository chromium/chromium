// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/kiosk/vision/kiosk_vision.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/kiosk/vision/internal/detection_processor.h"
#include "chromeos/ash/components/kiosk/vision/internal/pref_observer.h"
#include "chromeos/ash/components/kiosk/vision/pref_names.h"
#include "chromeos/ash/components/kiosk/vision/telemetry_processor.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace ash::kiosk_vision {

namespace {

void InstallDlc(base::OnceCallback<void(std::string dlc_root_path)> on_done) {
  auto& dlc_service = CHECK_DEREF(DlcserviceClient::Get());
  dlcservice::InstallRequest install_request;
  install_request.set_id(std::string(kKioskVisionDlcId));
  dlc_service.Install(
      install_request,
      base::BindOnce(
          [](base::OnceCallback<void(std::string)> on_done,
             const DlcserviceClient::InstallResult& result) {
            // TODO(b/334067044): Handle DLC install errors.
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
      kKioskVisionDlcId, base::BindOnce([](std::string_view error) {
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
  pref_observer_.Start();
}

KioskVision::~KioskVision() = default;

void KioskVision::Enable() {
  InstallDlc(/*on_done=*/base::BindOnce(&KioskVision::InitializeProcessors,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void KioskVision::Disable() {
  camera_connector_.reset();
  detection_observer_.reset();
  telemetry_processor_.reset();
}

void KioskVision::InitializeProcessors(std::string dlc_path) {
  telemetry_processor_.emplace();
  detection_observer_.emplace(
      DetectionProcessors({&telemetry_processor_.value()}));
  camera_connector_.emplace(std::move(dlc_path), &detection_observer_.value());
}

TelemetryProcessor* KioskVision::GetTelemetryProcessor() {
  return telemetry_processor_.has_value() ? &*telemetry_processor_ : nullptr;
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kKioskVisionTelemetryEnabled,
                                /*default_value=*/false);
}

}  // namespace ash::kiosk_vision
