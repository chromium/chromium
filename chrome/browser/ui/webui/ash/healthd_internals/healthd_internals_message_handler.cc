// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/healthd_internals/healthd_internals_message_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

namespace {

namespace mojom = ::ash::cros_healthd::mojom;

std::string Convert(mojom::ThermalSensorInfo::ThermalSensorSource source) {
  switch (source) {
    case mojom::ThermalSensorInfo::ThermalSensorSource::kEc:
      return "EC";
    case mojom::ThermalSensorInfo::ThermalSensorSource::kSysFs:
      return "Sysfs";
    case mojom::ThermalSensorInfo::ThermalSensorSource::kUnmappedEnumField:
      return "Unknown";
  }
}

base::Value::List ConvertThermalValue(const mojom::ThermalInfoPtr& info) {
  base::Value::List out_thermals;
  if (info) {
    for (const auto& thermal : info->thermal_sensors) {
      base::Value::Dict thermal_result;
      thermal_result.Set("name", thermal->name);
      thermal_result.Set("source", Convert(thermal->source));
      thermal_result.Set("temperatureCelsius", thermal->temperature_celsius);
      out_thermals.Append(std::move(thermal_result));
    }
  }
  return out_thermals;
}

}  // namespace

HealthdInternalsMessageHandler::HealthdInternalsMessageHandler() = default;

HealthdInternalsMessageHandler::~HealthdInternalsMessageHandler() = default;

void HealthdInternalsMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getHealthdTelemetryInfo",
      base::BindRepeating(
          &HealthdInternalsMessageHandler::HandleGetHealthdTelemetryInfo,
          weak_ptr_factory_.GetWeakPtr()));
}

void HealthdInternalsMessageHandler::HandleGetHealthdTelemetryInfo(
    const base::Value::List& list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  AllowJavascript();
  if (list.size() != 1 || !list[0].is_string()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  base::Value callback_id = list[0].Clone();
  auto* service = GetProbeService();
  if (!service) {
    HandleTelemetryResult(std::move(callback_id), nullptr);
    return;
  }

  service->ProbeTelemetryInfo(
      {mojom::ProbeCategoryEnum::kThermal},
      base::BindOnce(&HealthdInternalsMessageHandler::HandleTelemetryResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback_id)));
}

void HealthdInternalsMessageHandler::HandleTelemetryResult(
    base::Value callback_id,
    mojom::TelemetryInfoPtr info) {
  if (!info) {
    LOG(WARNING) << "Unable to access telemetry info from Healthd";
    ReplyHealthdInternalInfo(std::move(callback_id), base::Value::Dict());
    return;
  }

  base::Value::Dict result;
  if (info->thermal_result && info->thermal_result->is_thermal_info()) {
    result.Set("thermals",
               ConvertThermalValue(info->thermal_result->get_thermal_info()));
  }

  ReplyHealthdInternalInfo(std::move(callback_id), std::move(result));
}

void HealthdInternalsMessageHandler::ReplyHealthdInternalInfo(
    base::Value callback_id,
    base::Value::Dict result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ResolveJavascriptCallback(callback_id, result);
}

mojom::CrosHealthdProbeService*
HealthdInternalsMessageHandler::GetProbeService() {
  if (!probe_service_ || !probe_service_.is_connected()) {
    cros_healthd::ServiceConnection::GetInstance()->BindProbeService(
        probe_service_.BindNewPipeAndPassReceiver());
    probe_service_.set_disconnect_handler(base::BindOnce(
        &HealthdInternalsMessageHandler::OnProbeServiceDisconnect,
        weak_ptr_factory_.GetWeakPtr()));
  }
  return probe_service_.get();
}

void HealthdInternalsMessageHandler::OnProbeServiceDisconnect() {
  probe_service_.reset();
}

}  // namespace ash
