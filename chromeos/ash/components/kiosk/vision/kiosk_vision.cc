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
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/kiosk/vision/internal/camera_service_connector.h"
#include "chromeos/ash/components/kiosk/vision/internal/detection_processor.h"
#include "chromeos/ash/components/kiosk/vision/internal/pref_observer.h"
#include "chromeos/ash/components/kiosk/vision/internals_page_processor.h"
#include "chromeos/ash/components/kiosk/vision/pref_names.h"
#include "chromeos/ash/components/kiosk/vision/telemetry_processor.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace ash::kiosk_vision {

namespace {

void InstallDlc(base::OnceCallback<void(std::string dlc_root_path)> on_done,
                base::OnceClosure on_error) {
  auto& dlc_service = CHECK_DEREF(DlcserviceClient::Get());
  dlcservice::InstallRequest install_request;
  install_request.set_id(kKioskVisionDlcId);
  dlc_service.Install(
      install_request,
      base::BindOnce(
          [](base::OnceCallback<void(std::string)> on_done,
             base::OnceClosure on_error,
             const DlcserviceClient::InstallResult& result) {
            if (result.error != dlcservice::kErrorNone) {
              LOG(ERROR)
                  << "Kiosk Vision failed to install DLC, unable to proceed: "
                  << result.error;
              return std::move(on_error).Run();
            }

            std::move(on_done).Run(result.root_path);
          },
          std::move(on_done), std::move(on_error)),
      /*progress_callback=*/base::DoNothing());
}

void UninstallDlc() {
  auto& dlc_service = CHECK_DEREF(DlcserviceClient::Get());
  dlc_service.Uninstall(
      kKioskVisionDlcId, base::BindOnce([](std::string_view error) {
        if (error != dlcservice::kErrorNone) {
          LOG(WARNING) << "Kiosk Vision failed to uninstall DLC: " << error;
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
  InstallDlc(/*on_done=*/
             base::BindOnce(&KioskVision::InitializeProcessors,
                            weak_ptr_factory_.GetWeakPtr()),
             /*on_error=*/
             base::BindOnce(&KioskVision::OnDlcInstallError,
                            weak_ptr_factory_.GetWeakPtr()));
}

void KioskVision::Disable() {
  camera_connector_.reset();
  detection_observer_.reset();
  telemetry_processor_.reset();
  retry_timer_.Stop();
}

void KioskVision::InitializeProcessors(std::string dlc_path) {
  telemetry_processor_.emplace();
  DetectionProcessors ps = {&telemetry_processor_.value()};
  if (IsInternalsPageEnabled()) {
    internals_webui_processor_.emplace();
    ps.push_back(&internals_webui_processor_.value());
  }
  detection_observer_.emplace(std::move(ps));
  camera_connector_.emplace(std::move(dlc_path), &detection_observer_.value());
  camera_connector_->Start();
}

void KioskVision::OnDlcInstallError() {
  Disable();
  retry_timer_.Start(base::BindOnce(&KioskVision::Enable,
                                    // Safe because `this` owns `retry_timer_`.
                                    base::Unretained(this)));
}

TelemetryProcessor* KioskVision::GetTelemetryProcessor() {
  return telemetry_processor_.has_value() ? &telemetry_processor_.value()
                                          : nullptr;
}

InternalsPageProcessor* KioskVision::GetInternalsPageProcessor() {
  return internals_webui_processor_.has_value()
             ? &internals_webui_processor_.value()
             : nullptr;
}

const CameraServiceConnector* KioskVision::GetCameraConnectorForTesting()
    const {
  return camera_connector_.has_value() ? &camera_connector_.value() : nullptr;
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kKioskVisionTelemetryEnabled,
                                /*default_value=*/false);
  registry->RegisterTimeDeltaPref(prefs::kKioskVisionTelemetryFrequency,
                                  /*default_value=*/base::Minutes(2));
}

}  // namespace ash::kiosk_vision
