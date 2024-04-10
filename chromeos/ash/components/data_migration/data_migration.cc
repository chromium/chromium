// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/data_migration/data_migration.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/nearby/src/internal/platform/byte_array.h"
#include "third_party/nearby/src/internal/platform/byte_utils.h"

namespace data_migration {
namespace {

std::vector<uint8_t> BuildEndpointInfo() {
  // Must be < 131:
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/nearby/src/connections/implementation/bluetooth_device_name.h;l=70;drc=084f7ebd8847cfb68191f787dda192644377e6ad
  static constexpr size_t kEndpointInfoLength = 64;
  // TODO(esum): Fill with real content and comment. Currently, endpoint info
  // is still not used/required.
  return std::vector<uint8_t>(kEndpointInfoLength, 0);
}

}  // namespace

DataMigration::DataMigration(
    std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager)
    : nearby_connections_manager_(std::move(nearby_connections_manager)) {
  CHECK(nearby_connections_manager_);
}

// Do not use `nearby_connections_manager_` or anything that depends on it
// here. Any shutdown logic should go in `DataMigration::Shutdown()`.
DataMigration::~DataMigration() = default;

// Part of the KeyedService shutdown design. Must ensure all nearby connection
// activity is stopped here. Note `DataMigration` is expected to be destroyed
// shortly after this.
void DataMigration::Shutdown() {
  VLOG(4) << "__func__";
  // Ensure any pending callbacks do not run while tearing everything down.
  weak_factory_.InvalidateWeakPtrs();

  connected_device_.reset();
  nearby_connections_manager_->Shutdown();
}

void DataMigration::StartAdvertising() {
  VLOG(1) << "DataMigration advertising starting";
  CHECK(!connected_device_) << "Nearby connection already established";
  // `NearbyConnectionsManagerImpl` internally uses the arguments below to build
  // the following `AdvertisingOptions`:
  // * strategy - kP2pPointToPoint
  // * mediums
  //   * bluetooth - true
  //   * every other medium - false
  // * auto_upgrade_bandwidth - true
  // * enforce_topology_constraints - true
  // * enable_bluetooth_listening - false
  //   * From docs: "this allows listening on incoming Bluetooth Classic
  //     connections while BLE advertising". Should not be an issue because
  //     BLE is a disabled advertising medium to begin with.
  // * enable_webrtc_listening - false
  // * fast_advertisement_service_uuid - Some internal immutable value.
  nearby_connections_manager_->StartAdvertising(
      // `PowerLevel::kHighPower` matches what cros quick start uses and is
      // required by the `NearbyConnectionsManagerImpl` to use the bluetooth
      // classic medium.
      BuildEndpointInfo(), this,
      NearbyConnectionsManager::PowerLevel::kHighPower,
      // This causes `NearbyConnectionsManagerImpl` to disable all wifi-related
      // advertisement mechanisms (leaving only bluetooth classic). Note this
      // does not affect the medium for the main connection over which
      // payloads are transferred.
      nearby_share::mojom::DataUsage::kOffline,
      base::BindOnce(&DataMigration::OnStartAdvertising,
                     weak_factory_.GetWeakPtr()));
}

void DataMigration::OnStartAdvertising(
    NearbyConnectionsManager::ConnectionsStatus status) {
  if (status != NearbyConnectionsManager::ConnectionsStatus::kSuccess) {
    // See the `NearbyConnections::StartAdvertising()` API. None of the error
    // codes should apply here, but it's not worth crashing the browser process
    // in prod if this happens.
    // TODO(esum): Add metrics how often this is hit.
    LOG(DFATAL) << "DataMigration failed to start advertising with status="
                << status;
  }
}

void DataMigration::OnStopAdvertising(
    NearbyConnectionsManager::ConnectionsStatus status) {
  // Mojo docs claim this can never fail, but this is not worth crashing the
  // browser process even so. If advertising keeps running, it shouldn't cause
  // this class to fail; it's just less optimal.
  if (status != NearbyConnectionsManager::ConnectionsStatus::kSuccess) {
    LOG(DFATAL) << "DataMigration failed to stop advertising with status="
                << status;
  }
}

void DataMigration::OnIncomingConnectionInitiated(
    const std::string& endpoint_id,
    const std::vector<uint8_t>& endpoint_info) {
  // Note `NearbyConnectionsManagerImpl` automatically accepts this incoming
  // connection internally. This leaves the outcome in the hands of the remote
  // device, who must accept the connection as well before the 2 sides can start
  // exchanging payloads.
  //
  // This matches the DataMigration design because the user is expected to
  // verify that the 4 digit pin matches on both devices, and hit "accept" on
  // the remote device (the source of the data) for the connection to be
  // established.
  std::optional<std::vector<uint8_t>> auth_token =
      nearby_connections_manager_->GetRawAuthenticationToken(endpoint_id);
  CHECK(auth_token) << "Auth token missing. Should always be available because "
                       "connection was just initiated.";
  ::nearby::ByteArray auth_token_as_byte_array =
      ::nearby::ByteArray(std::string(auth_token->begin(), auth_token->end()));
  // TODO(esum):
  // * Check if `::nearby::ByteUtils::ToFourDigitString()` needs to run in a
  //   sandboxed process.
  // * Display the pin in an actual UI. Logs are used temporarily here for
  //   developers.
  // * Account for multiple incoming connection requests when the UI is built.
  VLOG(1) << "DataMigration connection requested with pin="
          << ::nearby::ByteUtils::ToFourDigitString(auth_token_as_byte_array);
}

void DataMigration::OnIncomingConnectionAccepted(
    const std::string& endpoint_id,
    const std::vector<uint8_t>& endpoint_info,
    NearbyConnection* connection) {
  if (connected_device_) {
    // Corner case should rarely happen, but only one data migration can be
    // active at a time.
    LOG(WARNING) << "DataMigration already active with another device. "
                    "Disconnecting from incoming endpoint.";
    connection->Close();
    return;
  }
  VLOG(1) << "DataMigration connection accepted";
  // Multiple parallel transfers is not supported, so there's no reason to
  // continue advertising at this point.
  nearby_connections_manager_->StopAdvertising(base::BindOnce(
      &DataMigration::OnStopAdvertising, weak_factory_.GetWeakPtr()));

  connected_device_.emplace(connection, nearby_connections_manager_.get());
  connection->SetDisconnectionListener(base::BindOnce(
      &DataMigration::OnDeviceDisconnected, weak_factory_.GetWeakPtr()));
}

void DataMigration::OnDeviceDisconnected() {
  CHECK(connected_device_);
  // Note this is not a transient disconnect. NC should handle transient network
  // errors internally. At this point, NC deems the connection unrecoverable
  // and its docs recommend starting the service discovery/advertising process
  // again.
  LOG(ERROR) << "DataMigration remote device has disconnected unexpectedly";
  connected_device_.reset();
  // Data Migration protocol does not persist state across connections. Once
  // the connection is dropped, the protocol resets. Clear any payloads in
  // memory that have not been processed yet since they are guaranteed to not be
  // used at this point. Any files that were completely transferred on disc will
  // be preserved though.
  nearby_connections_manager_->ClearIncomingPayloads();

  StartAdvertising();
}

}  // namespace data_migration
