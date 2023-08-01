// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_connections_conversions.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"

namespace nearby {
namespace connections {

Strategy StrategyFromMojom(mojom::Strategy strategy) {
  switch (strategy) {
    case mojom::Strategy::kP2pCluster:
      return Strategy::kP2pCluster;
    case mojom::Strategy::kP2pStar:
      return Strategy::kP2pStar;
    case mojom::Strategy::kP2pPointToPoint:
      return Strategy::kP2pPointToPoint;
  }
}

mojom::Status StatusToMojom(Status::Value status) {
  switch (status) {
    case Status::Value::kSuccess:
      return mojom::Status::kSuccess;
    case Status::Value::kError:
      return mojom::Status::kError;
    case Status::Value::kOutOfOrderApiCall:
      return mojom::Status::kOutOfOrderApiCall;
    case Status::Value::kAlreadyHaveActiveStrategy:
      return mojom::Status::kAlreadyHaveActiveStrategy;
    case Status::Value::kAlreadyAdvertising:
      return mojom::Status::kAlreadyAdvertising;
    case Status::Value::kAlreadyDiscovering:
      return mojom::Status::kAlreadyDiscovering;
    case Status::Value::kEndpointIoError:
      return mojom::Status::kEndpointIOError;
    case Status::Value::kEndpointUnknown:
      return mojom::Status::kEndpointUnknown;
    case Status::Value::kConnectionRejected:
      return mojom::Status::kConnectionRejected;
    case Status::Value::kAlreadyConnectedToEndpoint:
      return mojom::Status::kAlreadyConnectedToEndpoint;
    case Status::Value::kNotConnectedToEndpoint:
      return mojom::Status::kNotConnectedToEndpoint;
    case Status::Value::kBluetoothError:
      return mojom::Status::kBluetoothError;
    case Status::Value::kBleError:
      return mojom::Status::kBleError;
    case Status::Value::kWifiLanError:
      return mojom::Status::kWifiLanError;
    case Status::Value::kPayloadUnknown:
      return mojom::Status::kPayloadUnknown;
    case Status::Value::kAlreadyListening:
      return mojom::Status::kAlreadyListening;
    case Status::Value::kNextValue:
      return mojom::Status::kNextValue;
  }
}

ResultCallback ResultCallbackFromMojom(StatusCallback callback) {
  return [callback = std::move(callback),
          task_runner = base::SequencedTaskRunner::GetCurrentDefault()](
             Status status) mutable {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), StatusToMojom(status.value)));
  };
}

std::vector<uint8_t> ByteArrayToMojom(const ByteArray& byte_array) {
  return std::vector<uint8_t>(byte_array.data(),
                              byte_array.data() + byte_array.size());
}

ByteArray ByteArrayFromMojom(const std::vector<uint8_t>& byte_array) {
  return ByteArray(std::string(byte_array.begin(), byte_array.end()));
}

mojom::PayloadStatus PayloadStatusToMojom(PayloadProgressInfo::Status status) {
  switch (status) {
    case PayloadProgressInfo::Status::kSuccess:
      return mojom::PayloadStatus::kSuccess;
    case PayloadProgressInfo::Status::kFailure:
      return mojom::PayloadStatus::kFailure;
    case PayloadProgressInfo::Status::kInProgress:
      return mojom::PayloadStatus::kInProgress;
    case PayloadProgressInfo::Status::kCanceled:
      return mojom::PayloadStatus::kCanceled;
  }
}

mojom::Medium MediumToMojom(Medium medium) {
  switch (medium) {
    case Medium::UNKNOWN_MEDIUM:
      return mojom::Medium::kUnknown;
    case Medium::MDNS:
      return mojom::Medium::kMdns;
    case Medium::BLUETOOTH:
      return mojom::Medium::kBluetooth;
    case Medium::WIFI_HOTSPOT:
      return mojom::Medium::kWifiHotspot;
    case Medium::BLE:
      return mojom::Medium::kBle;
    case Medium::WIFI_LAN:
      return mojom::Medium::kWifiLan;
    case Medium::WIFI_AWARE:
      return mojom::Medium::kWifiAware;
    case Medium::NFC:
      return mojom::Medium::kNfc;
    case Medium::WIFI_DIRECT:
      return mojom::Medium::kWifiDirect;
    case Medium::WEB_RTC:
      return mojom::Medium::kWebRtc;
    case Medium::BLE_L2CAP:
      return mojom::Medium::kBleL2Cap;
    case Medium::USB:
      return mojom::Medium::kUsb;
  }
}

BooleanMediumSelector MediumSelectorFromMojom(
    mojom::MediumSelection* allowed_mediums) {
  return BooleanMediumSelector{
      .bluetooth = allowed_mediums->bluetooth,
      .ble = allowed_mediums->ble,
      .web_rtc = allowed_mediums->web_rtc,
      .wifi_lan = allowed_mediums->wifi_lan,
  };
}

}  // namespace connections
}  // namespace nearby
