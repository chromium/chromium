// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_TESTING_FAKE_NEARBY_CONNECTIONS_H_
#define CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_TESTING_FAKE_NEARBY_CONNECTIONS_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace data_migration {

// Purpose-built for data migration. Acts as the remote device (the one
// transferring data to the ChromeOS device) in tests.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION)
    FakeNearbyConnections
    : public ::nearby::connections::mojom::NearbyConnections {
 public:
  using PayloadStatus = ::nearby::connections::mojom::PayloadStatus;
  using Status = ::nearby::connections::mojom::Status;

  // `remote_endpoint_id` is the id of the simulated remote device from whom
  // data will be transferred.
  explicit FakeNearbyConnections(std::string_view remote_endpoint_id);
  FakeNearbyConnections(const FakeNearbyConnections&) = delete;
  FakeNearbyConnections& operator=(const FakeNearbyConnections&) = delete;
  ~FakeNearbyConnections() override;

  // Simulates a file being sent from the remote device (played by
  // `FakeNearbyConnections`) to the local device. The contents of the file
  // are randomly generated. All transferred file bytes are returned to the
  // caller via `transferred_bytes` if it's non-null.
  //
  // Returns true if file transmission was successfully simulated, false if any
  // error prevents the simulation.
  bool SendFile(int64_t payload_id,
                std::vector<uint8_t>* transferred_bytes = nullptr);

  // Sets the final payload status for all future `SendFile()` calls with
  // matching `payload_id`. Can be used to simulate file transfer failures.
  //
  // All file payloads with id not matching `payload_id` will return `kSuccess`.
  void SetFinalFilePayloadStatus(PayloadStatus status, int64_t payload_id);

  // The `register_payload_file_result_generator` is invoked for each call to
  // `RegisterPayloadFile()` and returns the `Status` of the operation.
  // By default, the generator is null and `RegisterPayloadFile()` succeeds.
  void set_register_payload_file_result_generator(
      base::RepeatingCallback<Status()>
          register_payload_file_result_generator) {
    register_payload_file_result_generator_ =
        std::move(register_payload_file_result_generator);
  }

  // Invoked every time a payload is sent from the local device to the remote
  // device.
  void set_local_to_remote_payload_listener(
      base::RepeatingCallback<void(::nearby::connections::mojom::PayloadPtr)>
          local_to_remote_payload_listener) {
    local_to_remote_payload_listener_ =
        std::move(local_to_remote_payload_listener);
  }

  // Invoked when both sides of the connection have been accepted, and payloads
  // can be exchanged.
  void set_connection_established_listener(
      base::RepeatingClosure connection_established_listener) {
    connection_established_listener_ =
        std::move(connection_established_listener);
  }

  // Size of all test files transmitted via `SendFile()`. Defaults to 1000.
  void set_test_file_size_in_bytes(size_t test_file_size_in_bytes) {
    test_file_size_in_bytes_ = test_file_size_in_bytes;
  }
  int test_file_size_in_bytes() const { return test_file_size_in_bytes_; }

  // Number of chunks into which files transmitted via `SendFile()` are divided.
  // Each chunk has the same size:
  // `test_file_size_in_bytes_` / `test_file_num_chunks_`.
  //
  // Defaults to 4.
  void set_test_file_num_chunks(size_t test_file_num_chunks) {
    test_file_num_chunks_ = test_file_num_chunks;
  }

  // Simulates a bytes payload being sent from the remote device to the local
  // device.
  //
  // Returns true if file transmission was successfully simulated, false if any
  // error prevents the simulation.
  [[nodiscard]] bool SendBytesPayload(int64_t payload_id,
                                      const std::string& bytes);

  // Simulates remote device disconnecting from the local device. Returns true
  // if simulation was successful; false if the remote device is not connected
  // to begin with.
  [[nodiscard]] bool SimulateRemoteDisconnect();

 private:
  // See `RegisterPayloadFile()` method.
  struct RegisteredFilePayload {
    RegisteredFilePayload();
    RegisteredFilePayload(base::File input_file_in, base::File output_file_in);
    RegisteredFilePayload(RegisteredFilePayload&&);
    RegisteredFilePayload& operator=(RegisteredFilePayload&&);
    ~RegisteredFilePayload();

    base::File input_file;
    base::File output_file;
  };

  // ::nearby::connections::mojom::NearbyConnections:
  void StartAdvertising(
      const std::string& service_id,
      const std::vector<uint8_t>& endpoint_info,
      ::nearby::connections::mojom::AdvertisingOptionsPtr options,
      mojo::PendingRemote<
          ::nearby::connections::mojom::ConnectionLifecycleListener> listener,
      StartAdvertisingCallback callback) override;
  void StopAdvertising(const std::string& service_id,
                       StopAdvertisingCallback callback) override;
  void StartDiscovery(
      const std::string& service_id,
      ::nearby::connections::mojom::DiscoveryOptionsPtr options,
      mojo::PendingRemote<
          ::nearby::connections::mojom::EndpointDiscoveryListener> listener,
      StartDiscoveryCallback callback) override;
  void StopDiscovery(const std::string& service_id,
                     StopDiscoveryCallback callback) override;
  void InjectBluetoothEndpoint(
      const std::string& service_id,
      const std::string& endpoint_id,
      const std::vector<uint8_t>& endpoint_info,
      const std::vector<uint8_t>& remote_bluetooth_mac_address,
      InjectBluetoothEndpointCallback callback) override;
  void RequestConnection(
      const std::string& service_id,
      const std::vector<uint8_t>& endpoint_info,
      const std::string& endpoint_id,
      ::nearby::connections::mojom::ConnectionOptionsPtr options,
      mojo::PendingRemote<
          ::nearby::connections::mojom::ConnectionLifecycleListener> listener,
      RequestConnectionCallback callback) override;
  void DisconnectFromEndpoint(const std::string& service_id,
                              const std::string& endpoint_id,
                              DisconnectFromEndpointCallback callback) override;
  void AcceptConnection(
      const std::string& service_id,
      const std::string& endpoint_id,
      mojo::PendingRemote<::nearby::connections::mojom::PayloadListener>
          listener,
      AcceptConnectionCallback callback) override;
  void RejectConnection(const std::string& service_id,
                        const std::string& endpoint_id,
                        RejectConnectionCallback callback) override;
  void SendPayload(const std::string& service_id,
                   const std::vector<std::string>& endpoint_ids,
                   ::nearby::connections::mojom::PayloadPtr payload,
                   SendPayloadCallback callback) override;
  void CancelPayload(const std::string& service_id,
                     int64_t payload_id,
                     CancelPayloadCallback callback) override;
  void StopAllEndpoints(const std::string& service_id,
                        StopAllEndpointsCallback callback) override;
  void InitiateBandwidthUpgrade(
      const std::string& service_id,
      const std::string& endpoint_id,
      InitiateBandwidthUpgradeCallback callback) override;
  void RegisterPayloadFile(const std::string& service_id,
                           int64_t payload_id,
                           base::File input_file,
                           base::File output_file,
                           RegisterPayloadFileCallback callback) override;
  void RequestConnectionV3(
      const std::string& service_id,
      ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
      ::nearby::connections::mojom::ConnectionOptionsPtr connection_options,
      mojo::PendingRemote<::nearby::connections::mojom::ConnectionListenerV3>
          listener,
      RequestConnectionV3Callback callback) override;
  void AcceptConnectionV3(
      const std::string& service_id,
      ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
      mojo::PendingRemote<::nearby::connections::mojom::PayloadListenerV3>
          listener,
      AcceptConnectionV3Callback callback) override;
  void RejectConnectionV3(
      const std::string& service_id,
      ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
      RejectConnectionV3Callback callback) override;
  void DisconnectFromDeviceV3(
      const std::string& service_id,
      ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
      DisconnectFromDeviceV3Callback callback) override;
  void RegisterServiceWithPresenceDeviceProvider(
      const std::string& service_id) override;

  const std::string remote_endpoint_id_;
  bool is_advertising_ = false;

  // Conceptually, both the `connection_listener_` and the
  // `remote_to_local_payload_listener_` are the target ChromeOS device that is
  // receiving data.
  //
  // Set during the discovery/advertising process.
  mojo::Remote<::nearby::connections::mojom::ConnectionLifecycleListener>
      connection_listener_;
  // Set during the payload transfer process (after connection is established).
  mojo::Remote<::nearby::connections::mojom::PayloadListener>
      remote_to_local_payload_listener_;

  base::flat_map</*payload_id*/ int64_t, RegisteredFilePayload>
      registered_files_;
  base::flat_map</*payload_id*/ int64_t, PayloadStatus>
      final_file_payload_status_;
  base::RepeatingCallback<Status()> register_payload_file_result_generator_;
  base::RepeatingCallback<void(::nearby::connections::mojom::PayloadPtr)>
      local_to_remote_payload_listener_;
  base::RepeatingClosure connection_established_listener_;

  size_t test_file_size_in_bytes_ = 1000;
  size_t test_file_num_chunks_ = 4;
};

}  // namespace data_migration

#endif  // CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_TESTING_FAKE_NEARBY_CONNECTIONS_H_
