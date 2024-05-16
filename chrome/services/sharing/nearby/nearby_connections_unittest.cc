// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_connections.h"

#include <stdint.h>

#include <sstream>

#include "ash/public/cpp/network_config_service.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/services/sharing/nearby/nearby_connections_conversions.h"
#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"
#include "chrome/services/sharing/nearby/test_support/mock_webrtc_dependencies.h"
#include "chromeos/ash/components/nearby/presence/conversions/nearby_presence_conversions.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_firewall_hole_factory.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_tcp_socket_factory.h"
#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/sharing.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/tcp_socket_factory.mojom.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/src/connections/implementation/mock_service_controller_router.h"
#include "third_party/nearby/src/connections/implementation/service_controller_router.h"
#include "third_party/nearby/src/connections/v3/bandwidth_info.h"
#include "third_party/nearby/src/connections/v3/connection_result.h"
#include "third_party/nearby/src/connections/v3/connections_device.h"
#include "third_party/nearby/src/connections/v3/listeners.h"
#include "third_party/nearby/src/internal/interop/fake_device_provider.h"

namespace nearby::connections {

using PresenceDevicePtr = ash::nearby::presence::mojom::PresenceDevicePtr;

namespace {

const char kServiceId[] = "NearbySharing";
const char kConnectionToken[] = "connection_token";
const char kFastAdvertisementServiceUuid[] =
    "0000fef3-0000-1000-8000-00805f9b34fb";
const char kEndpointId[] = "ABCD";
const size_t kEndpointIdLength = 4u;
const char kEndpointInfo[] = {0x0d, 0x07, 0x07, 0x07, 0x07};
const char kDeviceName[] = "Cris Cros's Pixel";
const std::vector<uint8_t> kDeviceId = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab,
                                        0xcd, 0xef, 0x01, 0x23, 0x45, 0x67,
                                        0x89, 0xab, 0xcd, 0xef};
const char kRemoteEndpointInfo[] = {0x0d, 0x07, 0x06, 0x08, 0x09};
const char kAuthenticationToken[] = "authentication_token";
const char kRawAuthenticationToken[] = {0x00, 0x05, 0x04, 0x03, 0x02};
const int64_t kPayloadId = 612721831;
const char kPayload[] = {0x0f, 0x0a, 0x0c, 0x0e};
const uint8_t kBluetoothMacAddress[] = {0x00, 0x00, 0xe6, 0x88, 0x64, 0x13};
const base::TimeDelta kKeepAliveInterval = base::Milliseconds(5123);
const base::TimeDelta kKeepAliveTimeout = base::Milliseconds(31234);

mojom::AdvertisingOptionsPtr CreateAdvertisingOptions() {
  bool use_ble = false;
  auto allowed_mediums = mojom::MediumSelection::New(/* bluetooth= */ true,
                                                     /* ble= */ use_ble,
                                                     /* web_rtc= */ false,
                                                     /* wifi_lan= */ true,
                                                     /* wifi_direct= */ true);
  return mojom::AdvertisingOptions::New(
      mojom::Strategy::kP2pPointToPoint, std::move(allowed_mediums),
      /* auto_upgrade_bandwidth= */ true,
      /* enforce_topology_constraints= */ true,
      /* enable_bluetooth_listening= */ use_ble,
      /* enable_webrtc_listening= */ false,
      /*fast_advertisement_service_uuid=*/
      device::BluetoothUUID(kFastAdvertisementServiceUuid));
}

mojom::ConnectionOptionsPtr CreateConnectionOptions(
    std::optional<std::vector<uint8_t>> bluetooth_mac_address,
    base::TimeDelta keep_alive_interval,
    base::TimeDelta keep_alive_timeout) {
  auto allowed_mediums = mojom::MediumSelection::New(/* bluetooth= */ true,
                                                     /* ble= */ false,
                                                     /* web_rtc= */ false,
                                                     /* wifi_lan= */ true,
                                                     /* wifi_direct= */ true);
  return mojom::ConnectionOptions::New(std::move(allowed_mediums),
                                       std::move(bluetooth_mac_address),
                                       keep_alive_interval, keep_alive_timeout);
}

struct EndpointData {
  std::string remote_endpoint_id;
  std::vector<uint8_t> remote_endpoint_info;
};

const EndpointData CreateEndpointData(int id) {
  EndpointData endpoint_data;

  // Create an endpoint ID of length |kEndpointIdLength| which consists of
  // |id| followed by spaces until the correct length is reached.
  std::stringstream ss;
  ss << id;
  while (ss.str().size() < kEndpointIdLength) {
    ss << " ";
  }
  endpoint_data.remote_endpoint_id = ss.str();

  endpoint_data.remote_endpoint_info = std::vector<uint8_t>(
      std::begin(kRemoteEndpointInfo), std::end(kRemoteEndpointInfo));
  endpoint_data.remote_endpoint_info.push_back(id);
  return endpoint_data;
}

nearby::internal::DeviceIdentityMetaData CreateMetadata() {
  nearby::internal::DeviceIdentityMetaData metadata;
  metadata.set_device_type(nearby::internal::DeviceType::DEVICE_TYPE_PHONE);
  metadata.set_device_name(kDeviceName);
  metadata.set_bluetooth_mac_address((char*)kBluetoothMacAddress);
  metadata.set_device_id(std::string(kDeviceId.begin(), kDeviceId.end()));
  return metadata;
}

v3::Quality GetMediumQuality(Medium medium) {
  switch (medium) {
    case Medium::USB:
    case Medium::UNKNOWN_MEDIUM:
      return v3::Quality::kUnknown;
    case Medium::BLE:
    case Medium::NFC:
      return v3::Quality::kLow;
    case Medium::BLUETOOTH:
    case Medium::BLE_L2CAP:
      return v3::Quality::kMedium;
    case Medium::WIFI_HOTSPOT:
    case Medium::WIFI_LAN:
    case Medium::WIFI_AWARE:
    case Medium::WIFI_DIRECT:
    case Medium::WEB_RTC:
      return v3::Quality::kHigh;
    default:
      return v3::Quality::kUnknown;
  }
}

}  // namespace

class FakeEndpointDiscoveryListener : public mojom::EndpointDiscoveryListener {
 public:
  void OnEndpointFound(const std::string& endpoint_id,
                       mojom::DiscoveredEndpointInfoPtr info) override {
    endpoint_found_cb.Run(endpoint_id, std::move(info));
  }

  void OnEndpointLost(const std::string& endpoint_id) override {
    endpoint_lost_cb.Run(endpoint_id);
  }

  mojo::Receiver<mojom::EndpointDiscoveryListener> receiver{this};
  base::RepeatingCallback<void(const std::string&,
                               mojom::DiscoveredEndpointInfoPtr)>
      endpoint_found_cb = base::DoNothing();
  base::RepeatingCallback<void(const std::string&)> endpoint_lost_cb =
      base::DoNothing();
};

class FakeConnectionLifecycleListener
    : public mojom::ConnectionLifecycleListener {
 public:
  void OnConnectionInitiated(const std::string& endpoint_id,
                             mojom::ConnectionInfoPtr info) override {
    initiated_cb.Run(endpoint_id, std::move(info));
  }

  void OnConnectionAccepted(const std::string& endpoint_id) override {
    accepted_cb.Run(endpoint_id);
  }

  void OnConnectionRejected(const std::string& endpoint_id,
                            mojom::Status status) override {
    rejected_cb.Run(endpoint_id, status);
  }

  void OnDisconnected(const std::string& endpoint_id) override {
    disconnected_cb.Run(endpoint_id);
  }

  void OnBandwidthChanged(const std::string& endpoint_id,
                          mojom::Medium medium) override {
    bandwidth_changed_cb.Run(endpoint_id, medium);
  }

  mojo::Receiver<mojom::ConnectionLifecycleListener> receiver{this};
  base::RepeatingCallback<void(const std::string&, mojom::ConnectionInfoPtr)>
      initiated_cb = base::DoNothing();
  base::RepeatingCallback<void(const std::string&)> accepted_cb =
      base::DoNothing();
  base::RepeatingCallback<void(const std::string&, mojom::Status)> rejected_cb =
      base::DoNothing();
  base::RepeatingCallback<void(const std::string&)> disconnected_cb =
      base::DoNothing();
  base::RepeatingCallback<void(const std::string&, mojom::Medium)>
      bandwidth_changed_cb = base::DoNothing();
};

class FakePayloadListener : public mojom::PayloadListener {
 public:
  void OnPayloadReceived(const std::string& endpoint_id,
                         mojom::PayloadPtr payload) override {
    payload_cb.Run(endpoint_id, std::move(payload));
  }

  void OnPayloadTransferUpdate(
      const std::string& endpoint_id,
      mojom::PayloadTransferUpdatePtr update) override {
    payload_progress_cb.Run(endpoint_id, std::move(update));
  }

  mojo::Receiver<mojom::PayloadListener> receiver{this};
  base::RepeatingCallback<void(const std::string&, mojom::PayloadPtr)>
      payload_cb = base::DoNothing();
  base::RepeatingCallback<void(const std::string&,
                               mojom::PayloadTransferUpdatePtr)>
      payload_progress_cb = base::DoNothing();
};

class FakeConnectionListenerV3 : public mojom::ConnectionListenerV3 {
 public:
  void OnConnectionInitiatedV3(
      const std::string& endpoint_id,
      mojom::InitialConnectionInfoV3Ptr info) override {
    initiated_cb.Run(endpoint_id, std::move(info));
  }

  void OnConnectionResultV3(const std::string& endpoint_id,
                            mojom::Status status) override {
    result_cb.Run(endpoint_id, status);
  }

  void OnDisconnectedV3(const std::string& endpoint_id) override {
    disconnected_cb.Run(endpoint_id);
  }

  void OnBandwidthChangedV3(const std::string& endpoint_id,
                            mojom::BandwidthInfoPtr bandwidth_info) override {
    bandwidth_changed_cb.Run(endpoint_id, std::move(bandwidth_info));
  }

  mojo::Receiver<mojom::ConnectionListenerV3> receiver{this};
  base::RepeatingCallback<void(const std::string&,
                               mojom::InitialConnectionInfoV3Ptr)>
      initiated_cb = base::DoNothing();
  base::RepeatingCallback<void(const std::string&, mojom::Status)> result_cb =
      base::DoNothing();
  base::RepeatingCallback<void(const std::string&)> disconnected_cb =
      base::DoNothing();
  base::RepeatingCallback<void(const std::string&, mojom::BandwidthInfoPtr)>
      bandwidth_changed_cb = base::DoNothing();
};

class FakePayloadListenerV3 : public mojom::PayloadListenerV3 {
 public:
  void OnPayloadReceivedV3(const std::string& endpoint_id,
                           mojom::PayloadPtr payload) override {
    payload_received_cb.Run(endpoint_id, std::move(payload));
  }

  void OnPayloadTransferUpdateV3(
      const std::string& endpoint_id,
      mojom::PayloadTransferUpdatePtr update) override {
    payload_progress_cb.Run(endpoint_id, std::move(update));
  }

  mojo::Receiver<mojom::PayloadListenerV3> receiver{this};
  base::RepeatingCallback<void(const std::string&, mojom::PayloadPtr)>
      payload_received_cb = base::DoNothing();
  base::RepeatingCallback<void(const std::string&,
                               mojom::PayloadTransferUpdatePtr)>
      payload_progress_cb = base::DoNothing();
};

using ::testing::_;
using ::testing::Return;
class MockInputStream : public InputStream {
 public:
  MOCK_METHOD(ExceptionOr<ByteArray>, Read, (std::int64_t), (override));
  MOCK_METHOD(Exception, Close, (), (override));
};

class NearbyConnectionsTest : public testing::Test {
 public:
  NearbyConnectionsTest() {
    auto service_controller_router =
        std::make_unique<testing::NiceMock<MockServiceControllerRouter>>();
    service_controller_router_ptr_ = service_controller_router.get();
    nearby_connections_ = std::make_unique<NearbyConnections>(
        remote_.BindNewPipeAndPassReceiver(), &fake_device_provider_,
        nearby::api::LogMessage::Severity::kInfo,
        base::BindOnce(&NearbyConnectionsTest::OnDisconnect,
                       base::Unretained(this)));
    nearby_connections_->SetServiceControllerRouterForTesting(
        std::move(service_controller_router));

    // Called when Cores are destroyed.
    ON_CALL(*service_controller_router_ptr_, StopAllEndpoints)
        .WillByDefault([&](ClientProxy* client, ResultCallback callback) {
          EXPECT_TRUE(callback);
          callback({Status::kSuccess});
        });
  }

  void OnDisconnect() { disconnect_run_loop_.Quit(); }

  ConnectionListener ConvertConnectionListenerV3ToV1(
      const NearbyDevice& remote_device,
      v3::ConnectionListener v3_connection_listener) {
    // `v3_connection_listener_` needs to be kept within scope of the test class
    // since we use a mock for the ServiceControllerRouter, which typically
    // maintains ownership/lifetime of the listener. Without this, the listener
    // would get invalidated after `OnConnectionInitiated()` completes.
    v3_connection_listener_ = std::move(v3_connection_listener);
    return ConnectionListener({
        .initiated_cb =
            [this, &remote_device](
                const std::string& endpoint_id,
                const ConnectionResponseInfo response_info) mutable {
              v3::InitialConnectionInfo new_info{
                  .authentication_digits = response_info.authentication_token,
                  .raw_authentication_token =
                      response_info.raw_authentication_token.string_data(),
                  .is_incoming_connection =
                      response_info.is_incoming_connection,
                  .authentication_status = response_info.authentication_status,
              };
              v3_connection_listener_.initiated_cb(remote_device, new_info);
            },
        .accepted_cb =
            [result_cb = v3_connection_listener_.result_cb](
                const std::string& endpoint_id) {
              v3::ConnectionResult result = {
                  .status = {Status::kSuccess},
              };
              result_cb(v3::ConnectionsDevice(endpoint_id, "", {}), result);
            },
        .rejected_cb =
            [result_cb = v3_connection_listener_.result_cb](
                const std::string& endpoint_id, Status status) {
              v3::ConnectionResult result = {
                  .status = status,
              };
              result_cb(v3::ConnectionsDevice(endpoint_id, "", {}), result);
            },
        .disconnected_cb =
            [this](const std::string& endpoint_id) mutable {
              v3_connection_listener_.disconnected_cb(
                  v3::ConnectionsDevice(endpoint_id, "", {}));
            },
        .bandwidth_changed_cb =
            [this](const std::string& endpoint_id, Medium medium) mutable {
              v3::BandwidthInfo bandwidth_info = {
                  .quality = GetMediumQuality(medium),
                  .medium = medium,
              };
              v3_connection_listener_.bandwidth_changed_cb(
                  v3::ConnectionsDevice(endpoint_id, "", {}), bandwidth_info);
            },
    });
  }

  PayloadListener ConvertPayloadListenerV3ToV1(
      v3::PayloadListener v3_payload_listener) {
    v3_payload_listener_ = std::move(v3_payload_listener);
    return PayloadListener({
        .payload_cb =
            [v3_received_cb =
                 std::move(v3_payload_listener_.payload_received_cb)](
                std::string_view endpoint_id, Payload payload) {
              v3_received_cb(v3::ConnectionsDevice(endpoint_id, "", {}),
                             std::move(payload));
            },
        .payload_progress_cb =
            [v3_progress_cb =
                 std::move(v3_payload_listener_.payload_progress_cb)](
                std::string_view endpoint_id,
                const PayloadProgressInfo& info) mutable {
              v3_progress_cb(v3::ConnectionsDevice(endpoint_id, "", {}), info);
            },
    });
  }

  ClientProxy* StartDiscovery(
      FakeEndpointDiscoveryListener& fake_discovery_listener,
      bool is_out_of_band_connection = false) {
    ClientProxy* client_proxy;
    EXPECT_CALL(*service_controller_router_ptr_, StartDiscovery)
        .WillOnce([&](ClientProxy* client, absl::string_view service_id,
                      const DiscoveryOptions& options,
                      DiscoveryListener listener, ResultCallback callback) {
          client_proxy = client;
          EXPECT_EQ(kServiceId, service_id);
          EXPECT_EQ(Strategy::kP2pPointToPoint, options.strategy);
          EXPECT_TRUE(options.allowed.bluetooth);
          EXPECT_FALSE(options.allowed.ble);
          EXPECT_FALSE(options.allowed.web_rtc);
          EXPECT_TRUE(options.allowed.wifi_lan);
          if (is_out_of_band_connection) {
            EXPECT_TRUE(options.is_out_of_band_connection);
          } else {
            EXPECT_FALSE(options.is_out_of_band_connection);
            EXPECT_EQ(kFastAdvertisementServiceUuid,
                      options.fast_advertisement_service_uuid);
          }
          client->StartedDiscovery(std::string{service_id}, options.strategy,
                                   std::move(listener),
                                   /* mediums= */ {});
          EXPECT_TRUE(callback);
          callback({Status::kAlreadyDiscovering});
        });
    base::RunLoop start_discovery_run_loop;
    nearby_connections_->StartDiscovery(
        kServiceId,
        mojom::DiscoveryOptions::New(
            mojom::Strategy::kP2pPointToPoint,
            mojom::MediumSelection::New(/* bluetooth= */ true,
                                        /* ble= */ false,
                                        /* web_rtc= */ false,
                                        /* wifi_lan= */ true,
                                        /* wifi_direct= */ true),
            device::BluetoothUUID(kFastAdvertisementServiceUuid),
            is_out_of_band_connection),
        fake_discovery_listener.receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting([&](mojom::Status status) {
          EXPECT_EQ(mojom::Status::kAlreadyDiscovering, status);
          start_discovery_run_loop.Quit();
        }));
    start_discovery_run_loop.Run();

    return client_proxy;
  }

  ClientProxy* StartAdvertising(
      FakeConnectionLifecycleListener& fake_connection_life_cycle_listener,
      const EndpointData& endpoint_data) {
    ClientProxy* client_proxy;
    std::vector<uint8_t> endpoint_info(std::begin(kEndpointInfo),
                                       std::end(kEndpointInfo));
    EXPECT_CALL(*service_controller_router_ptr_, StartAdvertising)
        .WillOnce([&](ClientProxy* client, absl::string_view service_id,
                      const AdvertisingOptions& options,
                      const ConnectionRequestInfo& info,
                      ResultCallback callback) {
          client_proxy = client;
          EXPECT_EQ(kServiceId, service_id);
          EXPECT_EQ(Strategy::kP2pPointToPoint, options.strategy);
          EXPECT_TRUE(options.allowed.bluetooth);
          EXPECT_FALSE(options.allowed.web_rtc);
          EXPECT_TRUE(options.allowed.wifi_lan);
          EXPECT_TRUE(options.auto_upgrade_bandwidth);
          EXPECT_TRUE(options.enforce_topology_constraints);
          EXPECT_EQ(endpoint_info, ByteArrayToMojom(info.endpoint_info));

          client_proxy->StartedAdvertising(std::string{service_id},
                                           options.strategy, info.listener,
                                           /* mediums= */ {});
          ConnectionOptions connection_options{
              .auto_upgrade_bandwidth = options.auto_upgrade_bandwidth,
              .enforce_topology_constraints =
                  options.enforce_topology_constraints,
              .enable_bluetooth_listening = options.enable_bluetooth_listening,
              .enable_webrtc_listening = options.enable_webrtc_listening,
              .fast_advertisement_service_uuid =
                  options.fast_advertisement_service_uuid};
          connection_options.strategy = options.strategy;
          connection_options.allowed = options.allowed;

          client_proxy->OnConnectionInitiated(
              endpoint_data.remote_endpoint_id,
              {.remote_endpoint_info =
                   ByteArrayFromMojom(endpoint_data.remote_endpoint_info),
               .authentication_token = kAuthenticationToken,
               .raw_authentication_token = ByteArray(
                   kRawAuthenticationToken, sizeof(kRawAuthenticationToken)),
               .is_incoming_connection = false},
              connection_options, info.listener, kConnectionToken);
          EXPECT_TRUE(callback);
          callback({Status::kSuccess});
        });

    base::RunLoop start_advertising_run_loop;
    nearby_connections_->StartAdvertising(
        kServiceId, endpoint_info, CreateAdvertisingOptions(),
        fake_connection_life_cycle_listener.receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting([&](mojom::Status status) {
          EXPECT_EQ(mojom::Status::kSuccess, status);
          start_advertising_run_loop.Quit();
        }));
    start_advertising_run_loop.Run();

    return client_proxy;
  }

  ClientProxy* RequestConnection(
      FakeConnectionLifecycleListener& fake_connection_life_cycle_listener,
      const EndpointData& endpoint_data,
      std::optional<std::vector<uint8_t>> bluetooth_mac_address =
          std::vector<uint8_t>(std::begin(kBluetoothMacAddress),
                               std::end(kBluetoothMacAddress))) {
    ClientProxy* client_proxy;
    std::vector<uint8_t> endpoint_info(std::begin(kEndpointInfo),
                                       std::end(kEndpointInfo));
    EXPECT_CALL(*service_controller_router_ptr_, RequestConnection)
        .WillOnce([&](ClientProxy* client, absl::string_view endpoint_id,
                      const ConnectionRequestInfo& info,
                      const ConnectionOptions& options,
                      ResultCallback callback) {
          client_proxy = client;
          EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
          EXPECT_EQ(endpoint_info, ByteArrayToMojom(info.endpoint_info));
          EXPECT_TRUE(options.allowed.bluetooth);
          EXPECT_FALSE(options.allowed.web_rtc);
          EXPECT_TRUE(options.allowed.wifi_lan);
          EXPECT_EQ(kKeepAliveInterval.InMilliseconds(),
                    options.keep_alive_interval_millis);
          EXPECT_EQ(kKeepAliveTimeout.InMilliseconds(),
                    options.keep_alive_timeout_millis);
          if (bluetooth_mac_address) {
            EXPECT_EQ(bluetooth_mac_address,
                      ByteArrayToMojom(options.remote_bluetooth_mac_address));
          } else {
            EXPECT_TRUE(options.remote_bluetooth_mac_address.Empty());
          }
          client_proxy->OnConnectionInitiated(
              std::string{endpoint_id},
              {.remote_endpoint_info =
                   ByteArrayFromMojom(endpoint_data.remote_endpoint_info),
               .authentication_token = kAuthenticationToken,
               .raw_authentication_token = ByteArray(
                   kRawAuthenticationToken, sizeof(kRawAuthenticationToken)),
               .is_incoming_connection = false},
              options, info.listener, kConnectionToken);
          EXPECT_TRUE(callback);
          callback({Status::kSuccess});
        });

    base::RunLoop request_connection_run_loop;
    nearby_connections_->RequestConnection(
        kServiceId, endpoint_info, endpoint_data.remote_endpoint_id,
        CreateConnectionOptions(bluetooth_mac_address, kKeepAliveInterval,
                                kKeepAliveTimeout),
        fake_connection_life_cycle_listener.receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting([&](mojom::Status status) {
          EXPECT_EQ(mojom::Status::kSuccess, status);
          request_connection_run_loop.Quit();
        }));
    request_connection_run_loop.Run();

    return client_proxy;
  }

  ClientProxy* AcceptConnection(FakePayloadListener& fake_payload_listener,
                                const std::string& remote_endpoint_id) {
    ClientProxy* client_proxy;
    EXPECT_CALL(*service_controller_router_ptr_, AcceptConnection)
        .WillOnce([&client_proxy, &remote_endpoint_id](
                      ClientProxy* client, absl::string_view endpoint_id,
                      PayloadListener listener, ResultCallback callback) {
          client_proxy = client;
          EXPECT_EQ(remote_endpoint_id, endpoint_id);
          client_proxy->LocalEndpointAcceptedConnection(
              std::string{endpoint_id}, std::move(listener));
          client_proxy->OnConnectionAccepted(std::string(endpoint_id));
          EXPECT_TRUE(callback);
          callback({Status::kSuccess});
        });

    base::RunLoop accept_connection_run_loop;
    nearby_connections_->AcceptConnection(
        kServiceId, remote_endpoint_id,
        fake_payload_listener.receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting([&](mojom::Status status) {
          EXPECT_EQ(mojom::Status::kSuccess, status);
          accept_connection_run_loop.Quit();
        }));
    accept_connection_run_loop.Run();

    return client_proxy;
  }

  ClientProxy* RequestConnectionV3(
      FakeConnectionListenerV3& fake_connection_listener_v3,
      PresenceDevicePtr remote_device,
      AuthenticationStatus authentication_status,
      std::optional<std::vector<uint8_t>> bluetooth_mac_address =
          std::vector<uint8_t>(std::begin(kBluetoothMacAddress),
                               std::end(kBluetoothMacAddress))) {
    ClientProxy* client_proxy;

    EXPECT_CALL(*service_controller_router_ptr_, RequestConnectionV3)
        .WillOnce([&](ClientProxy* client, const NearbyDevice& nearby_device,
                      v3::ConnectionRequestInfo info,
                      const ConnectionOptions& options,
                      ResultCallback callback) {
          client_proxy = client;

          EXPECT_TRUE(options.allowed.bluetooth);
          EXPECT_EQ(kKeepAliveInterval.InMilliseconds(),
                    options.keep_alive_interval_millis);
          EXPECT_EQ(kKeepAliveTimeout.InMilliseconds(),
                    options.keep_alive_timeout_millis);
          EXPECT_EQ(bluetooth_mac_address,
                    ByteArrayToMojom(options.remote_bluetooth_mac_address));
          EXPECT_EQ(kEndpointId, nearby_device.GetEndpointId());

          client_proxy->OnConnectionInitiated(
              std::string{nearby_device.GetEndpointId()},
              {.remote_endpoint_info = ByteArrayFromMojom(
                   std::vector<uint8_t>(std::begin(kRemoteEndpointInfo),
                                        std::end(kRemoteEndpointInfo))),
               .authentication_token = kAuthenticationToken,
               .raw_authentication_token = ByteArray(
                   kRawAuthenticationToken, sizeof(kRawAuthenticationToken)),
               .is_incoming_connection = false,
               .authentication_status = authentication_status},
              options,
              ConvertConnectionListenerV3ToV1(nearby_device,
                                              std::move(info.listener)),
              kConnectionToken);

          EXPECT_TRUE(callback);
          callback({Status::kSuccess});
        });

    base::RunLoop request_connection_run_loop;
    nearby_connections_->RequestConnectionV3(
        kServiceId, std::move(remote_device),
        CreateConnectionOptions(bluetooth_mac_address, kKeepAliveInterval,
                                kKeepAliveTimeout),
        fake_connection_listener_v3.receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting([&](mojom::Status status) {
          EXPECT_EQ(mojom::Status::kSuccess, status);
          request_connection_run_loop.Quit();
        }));
    request_connection_run_loop.Run();
    return client_proxy;
  }

  ClientProxy* AcceptConnectionV3(
      FakePayloadListenerV3& fake_payload_listener_v3,
      PresenceDevicePtr remote_device) {
    ClientProxy* client_proxy;

    EXPECT_CALL(*service_controller_router_ptr_, AcceptConnectionV3)
        .WillOnce([&client_proxy, this](
                      ClientProxy* client, const NearbyDevice& nearby_device,
                      v3::PayloadListener listener, ResultCallback callback) {
          client_proxy = client;

          EXPECT_EQ(kEndpointId, nearby_device.GetEndpointId());

          client_proxy->LocalEndpointAcceptedConnection(
              nearby_device.GetEndpointId(),
              ConvertPayloadListenerV3ToV1(std::move(listener)));
          client_proxy->OnConnectionAccepted(nearby_device.GetEndpointId());
          EXPECT_TRUE(callback);
          callback({Status::kSuccess});
        });

    base::RunLoop accept_connection_run_loop;
    nearby_connections_->AcceptConnectionV3(
        kServiceId, std::move(remote_device),
        fake_payload_listener_v3.receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting([&](mojom::Status status) {
          EXPECT_EQ(mojom::Status::kSuccess, status);
          accept_connection_run_loop.Quit();
        }));
    accept_connection_run_loop.Run();

    return client_proxy;
  }

  void RejectConnectionV3(PresenceDevicePtr remote_device) {
    ClientProxy* client_proxy;

    EXPECT_CALL(*service_controller_router_ptr_, RejectConnectionV3)
        .WillOnce([&client_proxy](ClientProxy* client,
                                  const NearbyDevice& nearby_device,
                                  ResultCallback callback) {
          client_proxy = client;

          EXPECT_EQ(kEndpointId, nearby_device.GetEndpointId());

          client_proxy->CancelEndpoint(nearby_device.GetEndpointId());
          EXPECT_TRUE(callback);
          callback({Status::kConnectionRejected});
        });

    base::RunLoop reject_connection_run_loop;
    nearby_connections_->RejectConnectionV3(
        kServiceId, std::move(remote_device),
        base::BindLambdaForTesting([&](mojom::Status status) {
          EXPECT_EQ(mojom::Status::kConnectionRejected, status);
          reject_connection_run_loop.Quit();
        }));
    reject_connection_run_loop.Run();
  }

  void VerifyRemoteDevice(
      const PresenceDevicePtr& remote_device,
      const nearby::presence::PresenceDevice& expected_device) {
    EXPECT_EQ(remote_device->endpoint_id, kEndpointId);
    EXPECT_EQ(remote_device->metadata->device_name,
              expected_device.GetDeviceIdentityMetadata().device_name());

    auto mac_addr_str =
        std::string(remote_device->metadata->bluetooth_mac_address.begin(),
                    remote_device->metadata->bluetooth_mac_address.end());
    EXPECT_EQ(
        mac_addr_str,
        expected_device.GetDeviceIdentityMetadata().bluetooth_mac_address());

    auto device_id_str = std::string(remote_device->metadata->device_id.begin(),
                                     remote_device->metadata->device_id.end());
    EXPECT_EQ(device_id_str,
              expected_device.GetDeviceIdentityMetadata().device_id());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::NearbyConnections> remote_;
  FakeDeviceProvider fake_device_provider_;
  bluetooth::FakeAdapter bluetooth_adapter_;
  ::sharing::MockWebRtcDependencies webrtc_dependencies_;
  std::unique_ptr<ash::network_config::CrosNetworkConfigTestHelper>
      cros_network_config_test_helper_;
  mojo::SelfOwnedReceiverRef<::sharing::mojom::FirewallHoleFactory>
      firewall_hole_factory_self_owned_receiver_ref_;
  mojo::SelfOwnedReceiverRef<::sharing::mojom::TcpSocketFactory>
      tcp_socket_factory_self_owned_receiver_ref_;
  std::unique_ptr<NearbyConnections> nearby_connections_;
  raw_ptr<testing::NiceMock<MockServiceControllerRouter>>
      service_controller_router_ptr_;
  v3::ConnectionListener v3_connection_listener_;
  v3::PayloadListener v3_payload_listener_;
  base::RunLoop disconnect_run_loop_;
};

TEST_F(NearbyConnectionsTest, RemoteDisconnect) {
  remote_.reset();
  disconnect_run_loop_.Run();
}

TEST_F(NearbyConnectionsTest, StartDiscovery) {
  FakeEndpointDiscoveryListener fake_discovery_listener;
  ClientProxy* client_proxy = StartDiscovery(fake_discovery_listener);

  base::RunLoop endpoint_found_run_loop;
  EndpointData endpoint_data = CreateEndpointData(1);
  fake_discovery_listener.endpoint_found_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id,
                                     mojom::DiscoveredEndpointInfoPtr info) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        EXPECT_EQ(endpoint_data.remote_endpoint_info, info->endpoint_info);
        EXPECT_EQ(kServiceId, info->service_id);
        endpoint_found_run_loop.Quit();
      });

  client_proxy->OnEndpointFound(
      kServiceId, endpoint_data.remote_endpoint_id,
      ByteArrayFromMojom(endpoint_data.remote_endpoint_info),
      /* medium= */ {});
  endpoint_found_run_loop.Run();

  base::RunLoop endpoint_lost_run_loop;
  fake_discovery_listener.endpoint_lost_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        endpoint_lost_run_loop.Quit();
      });
  client_proxy->OnEndpointLost(kServiceId, endpoint_data.remote_endpoint_id);
  endpoint_lost_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, StopDiscovery) {
  FakeEndpointDiscoveryListener fake_discovery_listener;
  StartDiscovery(fake_discovery_listener);

  EXPECT_CALL(*service_controller_router_ptr_, StopDiscovery)
      .WillOnce([](ClientProxy* client, ResultCallback callback) {
        EXPECT_TRUE(callback);
        callback({Status::kSuccess});
      });

  base::RunLoop stop_discovery_run_loop;
  nearby_connections_->StopDiscovery(
      kServiceId, base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        stop_discovery_run_loop.Quit();
      }));
  stop_discovery_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, InjectEndpoint) {
  const std::vector<uint8_t> bluetooth_mac_address(
      std::begin(kBluetoothMacAddress), std::end(kBluetoothMacAddress));
  const EndpointData endpoint_data = CreateEndpointData(1);

  base::RunLoop discovery_run_loop;
  FakeEndpointDiscoveryListener fake_discovery_listener;
  fake_discovery_listener.endpoint_found_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id,
                                     mojom::DiscoveredEndpointInfoPtr info) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        EXPECT_EQ(endpoint_data.remote_endpoint_info, info->endpoint_info);
        EXPECT_EQ(kServiceId, info->service_id);
        discovery_run_loop.Quit();
      });

  ClientProxy* client_proxy = StartDiscovery(
      fake_discovery_listener, /* is_out_of_band_connection= */ true);

  EXPECT_CALL(*service_controller_router_ptr_, InjectEndpoint)
      .WillOnce([&](ClientProxy* client, absl::string_view service_id,
                    const OutOfBandConnectionMetadata& metadata,
                    ResultCallback callback) {
        EXPECT_EQ(kServiceId, service_id);
        EXPECT_EQ(Medium::BLUETOOTH, metadata.medium);
        EXPECT_EQ(endpoint_data.remote_endpoint_id, metadata.endpoint_id);
        EXPECT_EQ(endpoint_data.remote_endpoint_info,
                  ByteArrayToMojom(metadata.endpoint_info));
        EXPECT_EQ(bluetooth_mac_address,
                  ByteArrayToMojom(metadata.remote_bluetooth_mac_address));
        client_proxy->OnEndpointFound(
            kServiceId, endpoint_data.remote_endpoint_id,
            ByteArrayFromMojom(endpoint_data.remote_endpoint_info),
            /* medium= */ {});
        EXPECT_TRUE(callback);
        callback({Status::kSuccess});
      });

  base::RunLoop inject_run_loop;
  nearby_connections_->InjectBluetoothEndpoint(
      kServiceId, endpoint_data.remote_endpoint_id,
      endpoint_data.remote_endpoint_info, bluetooth_mac_address,
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        inject_run_loop.Quit();
      }));

  discovery_run_loop.Run();
  inject_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, RequestConnectionInitiated) {
  FakeEndpointDiscoveryListener fake_discovery_listener;
  EndpointData endpoint_data = CreateEndpointData(1);
  ClientProxy* client_proxy = StartDiscovery(fake_discovery_listener);
  client_proxy->OnEndpointFound(
      kServiceId, endpoint_data.remote_endpoint_id,
      ByteArrayFromMojom(endpoint_data.remote_endpoint_info),
      /* medium= */ {});

  base::RunLoop initiated_run_loop;
  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  fake_connection_life_cycle_listener.initiated_cb = base::BindLambdaForTesting(
      [&](const std::string& endpoint_id, mojom::ConnectionInfoPtr info) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        EXPECT_EQ(kAuthenticationToken, info->authentication_token);
        EXPECT_EQ(std::vector<uint8_t>(std::begin(kRawAuthenticationToken),
                                       std::end(kRawAuthenticationToken)),
                  info->raw_authentication_token);
        EXPECT_EQ(endpoint_data.remote_endpoint_info, info->endpoint_info);
        EXPECT_FALSE(info->is_incoming_connection);
        initiated_run_loop.Quit();
      });

  RequestConnection(fake_connection_life_cycle_listener, endpoint_data);
  initiated_run_loop.Run();
}

TEST_F(NearbyConnectionsTest,
       RequestConnectionInitiatedWithoutBluetotohMacAddress) {
  FakeEndpointDiscoveryListener fake_discovery_listener;
  EndpointData endpoint_data = CreateEndpointData(1);
  ClientProxy* client_proxy = StartDiscovery(fake_discovery_listener);
  client_proxy->OnEndpointFound(
      kServiceId, endpoint_data.remote_endpoint_id,
      ByteArrayFromMojom(endpoint_data.remote_endpoint_info),
      /* medium= */ {});

  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;

  RequestConnection(fake_connection_life_cycle_listener, endpoint_data,
                    /* bluetooth_mac_address= */ std::nullopt);
}

TEST_F(NearbyConnectionsTest, RequestConnectionAccept) {
  FakeEndpointDiscoveryListener fake_discovery_listener;
  EndpointData endpoint_data = CreateEndpointData(1);
  ClientProxy* client_proxy = StartDiscovery(fake_discovery_listener);
  client_proxy->OnEndpointFound(
      kServiceId, endpoint_data.remote_endpoint_id,
      ByteArrayFromMojom(endpoint_data.remote_endpoint_info),
      /* medium= */ {});

  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  RequestConnection(fake_connection_life_cycle_listener, endpoint_data);

  base::RunLoop accepted_run_loop;
  fake_connection_life_cycle_listener.accepted_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        accepted_run_loop.Quit();
      });

  FakePayloadListener fake_payload_listener;
  AcceptConnection(fake_payload_listener, endpoint_data.remote_endpoint_id);
  accepted_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, RequestConnectionOnRejected) {
  FakeEndpointDiscoveryListener fake_discovery_listener;
  EndpointData endpoint_data = CreateEndpointData(1);
  ClientProxy* client_proxy = StartDiscovery(fake_discovery_listener);
  client_proxy->OnEndpointFound(
      kServiceId, endpoint_data.remote_endpoint_id,
      ByteArrayFromMojom(endpoint_data.remote_endpoint_info),
      /* medium= */ {});

  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  client_proxy =
      RequestConnection(fake_connection_life_cycle_listener, endpoint_data);

  base::RunLoop rejected_run_loop;
  fake_connection_life_cycle_listener.rejected_cb = base::BindLambdaForTesting(
      [&](const std::string& endpoint_id, mojom::Status status) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        EXPECT_EQ(mojom::Status::kConnectionRejected, status);
        rejected_run_loop.Quit();
      });

  client_proxy->OnConnectionRejected(endpoint_data.remote_endpoint_id,
                                     {Status::kConnectionRejected});
  rejected_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, RequestConnectionOnBandwidthUpgrade) {
  FakeEndpointDiscoveryListener fake_discovery_listener;
  EndpointData endpoint_data = CreateEndpointData(1);
  ClientProxy* client_proxy = StartDiscovery(fake_discovery_listener);
  client_proxy->OnEndpointFound(
      kServiceId, endpoint_data.remote_endpoint_id,
      ByteArrayFromMojom(endpoint_data.remote_endpoint_info),
      /* medium= */ {});

  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  RequestConnection(fake_connection_life_cycle_listener, endpoint_data);

  FakePayloadListener fake_payload_listener;
  client_proxy =
      AcceptConnection(fake_payload_listener, endpoint_data.remote_endpoint_id);

  // The life cycle listener should be triggered by a bandwidth upgrade.
  base::RunLoop upgraded_run_loop;
  fake_connection_life_cycle_listener.bandwidth_changed_cb =
      base::BindLambdaForTesting(
          [&](const std::string& endpoint_id, mojom::Medium medium) {
            EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
            EXPECT_EQ(mojom::Medium::kWebRtc, medium);
            upgraded_run_loop.Quit();
          });

  // Requesting a bandwidth upgrade should succeed.
  EXPECT_CALL(*service_controller_router_ptr_, InitiateBandwidthUpgrade)
      .WillOnce([&](ClientProxy* client, absl::string_view endpoint_id,
                    ResultCallback callback) {
        client_proxy = client;
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        client_proxy->OnBandwidthChanged(std::string{endpoint_id},
                                         Medium::WEB_RTC);
        EXPECT_TRUE(callback);
        callback({Status::kSuccess});
      });
  base::RunLoop bandwidth_upgrade_run_loop;
  nearby_connections_->InitiateBandwidthUpgrade(
      kServiceId, endpoint_data.remote_endpoint_id,
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        bandwidth_upgrade_run_loop.Quit();
      }));
  bandwidth_upgrade_run_loop.Run();

  upgraded_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, RequestConnectionOnDisconnected) {
  FakeEndpointDiscoveryListener fake_discovery_listener;
  EndpointData endpoint_data = CreateEndpointData(1);
  ClientProxy* client_proxy = StartDiscovery(fake_discovery_listener);
  client_proxy->OnEndpointFound(
      kServiceId, endpoint_data.remote_endpoint_id,
      ByteArrayFromMojom(endpoint_data.remote_endpoint_info),
      /* medium= */ {});

  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  RequestConnection(fake_connection_life_cycle_listener, endpoint_data);

  FakePayloadListener fake_payload_listener;
  client_proxy =
      AcceptConnection(fake_payload_listener, endpoint_data.remote_endpoint_id);

  base::RunLoop disconnected_run_loop;
  fake_connection_life_cycle_listener.disconnected_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        disconnected_run_loop.Quit();
      });

  client_proxy->OnDisconnected(endpoint_data.remote_endpoint_id,
                               /* notify= */ true);
  disconnected_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, RequestConnectionDisconnect) {
  FakeEndpointDiscoveryListener fake_discovery_listener;
  EndpointData endpoint_data = CreateEndpointData(1);
  ClientProxy* client_proxy = StartDiscovery(fake_discovery_listener);
  client_proxy->OnEndpointFound(
      kServiceId, endpoint_data.remote_endpoint_id,
      ByteArrayFromMojom(endpoint_data.remote_endpoint_info),
      /* medium= */ {});

  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  RequestConnection(fake_connection_life_cycle_listener, endpoint_data);

  FakePayloadListener fake_payload_listener;
  AcceptConnection(fake_payload_listener, endpoint_data.remote_endpoint_id);

  EXPECT_CALL(*service_controller_router_ptr_, DisconnectFromEndpoint)
      .WillOnce([&](ClientProxy* client, absl::string_view endpoint_id,
                    ResultCallback callback) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, std::string(endpoint_id));
        client->OnDisconnected(std::string{endpoint_id}, /* notify= */ true);
        EXPECT_TRUE(callback);
        callback({Status::kSuccess});
      });

  base::RunLoop disconnected_run_loop;
  fake_connection_life_cycle_listener.disconnected_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        disconnected_run_loop.Quit();
      });

  base::RunLoop disconnect_from_endpoint_run_loop;
  nearby_connections_->DisconnectFromEndpoint(
      kServiceId, endpoint_data.remote_endpoint_id,
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        disconnect_from_endpoint_run_loop.Quit();
      }));
  disconnect_from_endpoint_run_loop.Run();
  disconnected_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, OnPayloadTransferUpdate) {
  FakeEndpointDiscoveryListener fake_discovery_listener;
  EndpointData endpoint_data = CreateEndpointData(1);
  ClientProxy* client_proxy = StartDiscovery(fake_discovery_listener);
  client_proxy->OnEndpointFound(
      kServiceId, endpoint_data.remote_endpoint_id,
      ByteArrayFromMojom(endpoint_data.remote_endpoint_info),
      /* medium= */ {});

  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  RequestConnection(fake_connection_life_cycle_listener, endpoint_data);

  FakePayloadListener fake_payload_listener;
  client_proxy =
      AcceptConnection(fake_payload_listener, endpoint_data.remote_endpoint_id);

  base::RunLoop payload_progress_run_loop;
  fake_payload_listener.payload_progress_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id,
                                     mojom::PayloadTransferUpdatePtr info) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        payload_progress_run_loop.Quit();
      });

  client_proxy->OnPayloadProgress(endpoint_data.remote_endpoint_id, {});
  payload_progress_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, SendBytesPayload) {
  const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                              std::end(kPayload));

  FakeEndpointDiscoveryListener fake_discovery_listener;
  EndpointData endpoint_data = CreateEndpointData(1);
  ClientProxy* client_proxy = StartDiscovery(fake_discovery_listener);
  client_proxy->OnEndpointFound(
      kServiceId, endpoint_data.remote_endpoint_id,
      ByteArrayFromMojom(endpoint_data.remote_endpoint_info),
      /* medium= */ {});

  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  RequestConnection(fake_connection_life_cycle_listener, endpoint_data);

  FakePayloadListener fake_payload_listener;
  AcceptConnection(fake_payload_listener, endpoint_data.remote_endpoint_id);

  EXPECT_CALL(*service_controller_router_ptr_, SendPayload)
      .WillOnce([&](ClientProxy* client,
                    absl::Span<const std::string> endpoint_ids, Payload payload,
                    ResultCallback callback) {
        ASSERT_EQ(1u, endpoint_ids.size());
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_ids.front());
        EXPECT_EQ(PayloadType::kBytes, payload.GetType());
        std::string payload_bytes(payload.AsBytes());
        EXPECT_EQ(expected_payload, ByteArrayToMojom(payload.AsBytes()));
        EXPECT_TRUE(callback);
        callback({Status::kSuccess});
      });

  base::RunLoop send_payload_run_loop;
  nearby_connections_->SendPayload(
      kServiceId, {endpoint_data.remote_endpoint_id},
      mojom::Payload::New(kPayloadId,
                          mojom::PayloadContent::NewBytes(
                              mojom::BytesPayload::New(expected_payload))),
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        send_payload_run_loop.Quit();
      }));
  send_payload_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, SendBytesPayloadCancelled) {
  const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                              std::end(kPayload));

  FakeEndpointDiscoveryListener fake_discovery_listener;
  ClientProxy* client_proxy = StartDiscovery(fake_discovery_listener);
  EndpointData endpoint_data = CreateEndpointData(1);
  client_proxy->OnEndpointFound(
      kServiceId, endpoint_data.remote_endpoint_id,
      ByteArrayFromMojom(endpoint_data.remote_endpoint_info),
      /* medium= */ {});

  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  client_proxy =
      RequestConnection(fake_connection_life_cycle_listener, endpoint_data);

  FakePayloadListener fake_payload_listener;
  client_proxy =
      AcceptConnection(fake_payload_listener, endpoint_data.remote_endpoint_id);

  EXPECT_CALL(*service_controller_router_ptr_, SendPayload)
      .WillOnce([&](ClientProxy* client,
                    absl::Span<const std::string> endpoint_ids, Payload payload,
                    ResultCallback callback) {
        ASSERT_EQ(1u, endpoint_ids.size());
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_ids.front());
        EXPECT_EQ(PayloadType::kBytes, payload.GetType());
        std::string payload_bytes(payload.AsBytes());
        EXPECT_EQ(expected_payload, ByteArrayToMojom(payload.AsBytes()));
        EXPECT_TRUE(callback);
        callback({Status::kSuccess});
      });

  base::RunLoop send_payload_run_loop;
  nearby_connections_->SendPayload(
      kServiceId, {endpoint_data.remote_endpoint_id},
      mojom::Payload::New(kPayloadId,
                          mojom::PayloadContent::NewBytes(
                              mojom::BytesPayload::New(expected_payload))),
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        send_payload_run_loop.Quit();
      }));
  send_payload_run_loop.Run();

  EXPECT_CALL(
      *service_controller_router_ptr_,
      CancelPayload(testing::_, testing::Eq((uint64_t)kPayloadId), testing::_))
      .WillOnce([&](ClientProxy* client, std::uint64_t payload_id,
                    ResultCallback callback) {
        EXPECT_TRUE(callback);
        callback({Status::kSuccess});
      });

  base::RunLoop cancel_payload_run_loop;
  nearby_connections_->CancelPayload(
      kServiceId, kPayloadId,
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        cancel_payload_run_loop.Quit();
      }));
  cancel_payload_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, SendFilePayload) {
  const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                              std::end(kPayload));

  FakeEndpointDiscoveryListener fake_discovery_listener;
  EndpointData endpoint_data = CreateEndpointData(1);
  ClientProxy* client_proxy = StartDiscovery(fake_discovery_listener);
  client_proxy->OnEndpointFound(
      kServiceId, endpoint_data.remote_endpoint_id,
      ByteArrayFromMojom(endpoint_data.remote_endpoint_info),
      /* medium= */ {});

  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  RequestConnection(fake_connection_life_cycle_listener, endpoint_data);

  FakePayloadListener fake_payload_listener;
  AcceptConnection(fake_payload_listener, endpoint_data.remote_endpoint_id);

  EXPECT_CALL(*service_controller_router_ptr_, SendPayload)
      .WillOnce([&](ClientProxy* client,
                    absl::Span<const std::string> endpoint_ids, Payload payload,
                    ResultCallback callback) {
        ASSERT_EQ(1u, endpoint_ids.size());
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_ids.front());
        EXPECT_EQ(PayloadType::kFile, payload.GetType());
        InputFile* file = payload.AsFile();
        ASSERT_TRUE(file);
        ExceptionOr<ByteArray> bytes = file->Read(file->GetTotalSize());
        ASSERT_TRUE(bytes.ok());
        EXPECT_EQ(expected_payload, ByteArrayToMojom(bytes.result()));
        EXPECT_TRUE(callback);
        callback({Status::kSuccess});
      });

  base::FilePath path;
  EXPECT_TRUE(base::CreateTemporaryFile(&path));
  base::File output_file(path, base::File::Flags::FLAG_CREATE_ALWAYS |
                                   base::File::Flags::FLAG_WRITE);
  ASSERT_TRUE(output_file.IsValid());
  EXPECT_TRUE(output_file.WriteAndCheck(
      /* offset= */ 0, base::make_span(expected_payload)));
  EXPECT_TRUE(output_file.Flush());
  output_file.Close();

  base::File input_file(
      path, base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  EXPECT_TRUE(input_file.IsValid());

  base::RunLoop send_payload_run_loop;
  nearby_connections_->SendPayload(
      kServiceId, {endpoint_data.remote_endpoint_id},
      mojom::Payload::New(kPayloadId,
                          mojom::PayloadContent::NewFile(
                              mojom::FilePayload::New(std::move(input_file)))),
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        send_payload_run_loop.Quit();
      }));
  send_payload_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, StartAdvertisingRejected) {
  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  EndpointData endpoint_data = CreateEndpointData(1);

  base::RunLoop initiated_run_loop;
  fake_connection_life_cycle_listener.initiated_cb = base::BindLambdaForTesting(
      [&](const std::string& endpoint_id, mojom::ConnectionInfoPtr info) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        EXPECT_EQ(kAuthenticationToken, info->authentication_token);
        EXPECT_EQ(std::vector<uint8_t>(std::begin(kRawAuthenticationToken),
                                       std::end(kRawAuthenticationToken)),
                  info->raw_authentication_token);
        EXPECT_EQ(endpoint_data.remote_endpoint_info, info->endpoint_info);
        EXPECT_FALSE(info->is_incoming_connection);
        initiated_run_loop.Quit();
      });

  ClientProxy* client_proxy =
      StartAdvertising(fake_connection_life_cycle_listener, endpoint_data);
  initiated_run_loop.Run();

  base::RunLoop rejected_run_loop;
  fake_connection_life_cycle_listener.rejected_cb = base::BindLambdaForTesting(
      [&](const std::string& endpoint_id, mojom::Status status) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        EXPECT_EQ(mojom::Status::kConnectionRejected, status);
        rejected_run_loop.Quit();
      });
  client_proxy->OnConnectionRejected(endpoint_data.remote_endpoint_id,
                                     {Status::kConnectionRejected});
  rejected_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, StartAdvertisingAccepted) {
  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  EndpointData endpoint_data = CreateEndpointData(1);

  base::RunLoop initiated_run_loop;
  fake_connection_life_cycle_listener.initiated_cb = base::BindLambdaForTesting(
      [&](const std::string& endpoint_id, mojom::ConnectionInfoPtr info) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        EXPECT_EQ(kAuthenticationToken, info->authentication_token);
        EXPECT_EQ(std::vector<uint8_t>(std::begin(kRawAuthenticationToken),
                                       std::end(kRawAuthenticationToken)),
                  info->raw_authentication_token);
        EXPECT_EQ(endpoint_data.remote_endpoint_info, info->endpoint_info);
        EXPECT_FALSE(info->is_incoming_connection);
        initiated_run_loop.Quit();
      });

  StartAdvertising(fake_connection_life_cycle_listener, endpoint_data);
  initiated_run_loop.Run();

  base::RunLoop accepted_run_loop;
  fake_connection_life_cycle_listener.accepted_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        accepted_run_loop.Quit();
      });

  FakePayloadListener fake_payload_listener;
  AcceptConnection(fake_payload_listener, endpoint_data.remote_endpoint_id);
  accepted_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, StopAdvertising) {
  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  EndpointData endpoint_data = CreateEndpointData(1);
  StartAdvertising(fake_connection_life_cycle_listener, endpoint_data);

  EXPECT_CALL(*service_controller_router_ptr_, StopAdvertising)
      .WillOnce([](ClientProxy* client, ResultCallback callback) {
        EXPECT_TRUE(callback);
        callback({Status::kSuccess});
      });

  base::RunLoop stop_advertising_run_loop;
  nearby_connections_->StopAdvertising(
      kServiceId, base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        stop_advertising_run_loop.Quit();
      }));
  stop_advertising_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, DisconnectAllEndpoints) {
  FakeEndpointDiscoveryListener fake_discovery_listener;
  EndpointData endpoint_data = CreateEndpointData(1);
  ClientProxy* client_proxy = StartDiscovery(fake_discovery_listener);
  client_proxy->OnEndpointFound(
      kServiceId, endpoint_data.remote_endpoint_id,
      ByteArrayFromMojom(endpoint_data.remote_endpoint_info),
      /* medium= */ {});

  // Set up a connection to one endpoint.
  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  ConnectionListener connections_listener;
  RequestConnection(fake_connection_life_cycle_listener, endpoint_data);

  FakePayloadListener fake_payload_listener;
  AcceptConnection(fake_payload_listener, endpoint_data.remote_endpoint_id);

  // Set up a pending connection to a different endpoint.
  EndpointData endpoint_data2 = CreateEndpointData(2);
  client_proxy->OnEndpointFound(
      kServiceId, endpoint_data2.remote_endpoint_id,
      ByteArrayFromMojom(endpoint_data2.remote_endpoint_info),
      /* medium= */ {});

  FakeConnectionLifecycleListener fake_connection_life_cycle_listener2;
  ConnectionListener connections_listener2;
  RequestConnection(fake_connection_life_cycle_listener2, endpoint_data2);

  EXPECT_CALL(*service_controller_router_ptr_, StopAllEndpoints)
      .Times(2)
      .WillRepeatedly([&](ClientProxy* client, ResultCallback callback) {
        EXPECT_TRUE(callback);
        callback({Status::kSuccess});
      });

  base::RunLoop stop_endpoints_run_loop;
  nearby_connections_->StopAllEndpoints(
      kServiceId, base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        stop_endpoints_run_loop.Quit();
      }));
  stop_endpoints_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, InitiateBandwidthUpgrade) {
  EndpointData endpoint_data = CreateEndpointData(1);
  EXPECT_CALL(*service_controller_router_ptr_, InitiateBandwidthUpgrade)
      .WillOnce([&](ClientProxy* client, absl::string_view endpoint_id,
                    ResultCallback callback) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        EXPECT_TRUE(callback);
        callback({Status::kSuccess});
      });
  base::RunLoop bandwidth_upgrade_run_loop;
  nearby_connections_->InitiateBandwidthUpgrade(
      kServiceId, endpoint_data.remote_endpoint_id,
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        bandwidth_upgrade_run_loop.Quit();
      }));
  bandwidth_upgrade_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, InitiateBandwidthUpgradeFails) {
  EndpointData endpoint_data = CreateEndpointData(1);
  EXPECT_CALL(*service_controller_router_ptr_, InitiateBandwidthUpgrade)
      .WillOnce([&](ClientProxy* client, absl::string_view endpoint_id,
                    ResultCallback callback) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        EXPECT_TRUE(callback);
        callback({Status::kOutOfOrderApiCall});
      });
  base::RunLoop bandwidth_upgrade_run_loop;
  nearby_connections_->InitiateBandwidthUpgrade(
      kServiceId, endpoint_data.remote_endpoint_id,
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kOutOfOrderApiCall, status);
        bandwidth_upgrade_run_loop.Quit();
      }));
  bandwidth_upgrade_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, ReceiveBytesPayload) {
  const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                              std::end(kPayload));
  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  EndpointData endpoint_data = CreateEndpointData(1);
  StartAdvertising(fake_connection_life_cycle_listener, endpoint_data);

  base::RunLoop accepted_run_loop;
  fake_connection_life_cycle_listener.accepted_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        accepted_run_loop.Quit();
      });

  FakePayloadListener fake_payload_listener;
  ClientProxy* client_proxy =
      AcceptConnection(fake_payload_listener, endpoint_data.remote_endpoint_id);
  accepted_run_loop.Run();

  base::RunLoop payload_run_loop;
  fake_payload_listener.payload_cb = base::BindLambdaForTesting(
      [&](const std::string& endpoint_id, mojom::PayloadPtr payload) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        EXPECT_EQ(kPayloadId, payload->id);
        ASSERT_TRUE(payload->content->is_bytes());
        EXPECT_EQ(expected_payload, payload->content->get_bytes()->bytes);
        payload_run_loop.Quit();
      });

  client_proxy->OnPayload(
      endpoint_data.remote_endpoint_id,
      Payload(kPayloadId, ByteArrayFromMojom(expected_payload)));
  payload_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, ReceiveFilePayload) {
  const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                              std::end(kPayload));
  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  EndpointData endpoint_data = CreateEndpointData(1);
  StartAdvertising(fake_connection_life_cycle_listener, endpoint_data);

  base::RunLoop accepted_run_loop;
  fake_connection_life_cycle_listener.accepted_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        accepted_run_loop.Quit();
      });

  FakePayloadListener fake_payload_listener;
  ClientProxy* client_proxy =
      AcceptConnection(fake_payload_listener, endpoint_data.remote_endpoint_id);
  accepted_run_loop.Run();

  base::FilePath path;
  EXPECT_TRUE(base::CreateTemporaryFile(&path));
  base::File output_file(path, base::File::Flags::FLAG_CREATE_ALWAYS |
                                   base::File::Flags::FLAG_WRITE);
  EXPECT_TRUE(output_file.IsValid());
  base::File input_file(
      path, base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  EXPECT_TRUE(input_file.IsValid());

  base::RunLoop register_payload_run_loop;
  nearby_connections_->RegisterPayloadFile(
      kServiceId, kPayloadId, std::move(input_file), std::move(output_file),
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        register_payload_run_loop.Quit();
      }));
  register_payload_run_loop.Run();

  // Can start writing to OutputFile once registered.
  OutputFile core_output_file(kPayloadId);
  EXPECT_TRUE(
      core_output_file.Write(ByteArrayFromMojom(expected_payload)).Ok());
  EXPECT_TRUE(core_output_file.Flush().Ok());
  EXPECT_TRUE(core_output_file.Close().Ok());

  base::RunLoop payload_run_loop;
  fake_payload_listener.payload_cb = base::BindLambdaForTesting(
      [&](const std::string& endpoint_id, mojom::PayloadPtr payload) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        EXPECT_EQ(kPayloadId, payload->id);
        ASSERT_TRUE(payload->content->is_file());

        base::File& file = payload->content->get_file()->file;
        std::vector<uint8_t> buffer(file.GetLength());
        EXPECT_TRUE(
            file.ReadAndCheck(/* offset= */ 0, base::make_span(buffer)));
        EXPECT_EQ(expected_payload, buffer);

        payload_run_loop.Quit();
      });

  client_proxy->OnPayload(
      endpoint_data.remote_endpoint_id,
      Payload(kPayloadId, InputFile(kPayloadId, expected_payload.size())));
  payload_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, ReceiveFilePayloadNotRegistered) {
  const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                              std::end(kPayload));
  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  EndpointData endpoint_data = CreateEndpointData(1);
  StartAdvertising(fake_connection_life_cycle_listener, endpoint_data);

  base::RunLoop accepted_run_loop;
  fake_connection_life_cycle_listener.accepted_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        accepted_run_loop.Quit();
      });

  FakePayloadListener fake_payload_listener;
  ClientProxy* client_proxy =
      AcceptConnection(fake_payload_listener, endpoint_data.remote_endpoint_id);
  accepted_run_loop.Run();

  fake_payload_listener.payload_cb = base::BindLambdaForTesting(
      [&](const std::string& endpoint_id, mojom::PayloadPtr payload) {
        NOTREACHED_IN_MIGRATION();
      });

  EXPECT_CALL(
      *service_controller_router_ptr_,
      CancelPayload(testing::_, testing::Eq((uint64_t)kPayloadId), testing::_))
      .WillOnce([&](ClientProxy* client, std::uint64_t payload_id,
                    ResultCallback callback) {
        // Since ResultCallback is absl::AnyInvocable(), it may be invalid/not
        // callable. Must do a full check else the callback will crash.
        if (callback) {
          callback({Status::kSuccess});
        }
      });

  client_proxy->OnPayload(
      endpoint_data.remote_endpoint_id,
      Payload(kPayloadId, InputFile(kPayloadId, expected_payload.size())));

  // All file operations will throw IOException.
  OutputFile core_output_file(kPayloadId);
  EXPECT_TRUE(core_output_file.Write(ByteArrayFromMojom(expected_payload))
                  .Raised(Exception::kIo));
  EXPECT_TRUE(core_output_file.Flush().Raised(Exception::kIo));
  EXPECT_TRUE(core_output_file.Close().Raised(Exception::kIo));
}

TEST_F(NearbyConnectionsTest, RegisterPayloadFileInvalid) {
  base::RunLoop register_payload_run_loop;
  nearby_connections_->RegisterPayloadFile(
      kServiceId, kPayloadId, base::File(), base::File(),
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kError, status);
        register_payload_run_loop.Quit();
      }));
  register_payload_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, ReceiveStreamPayload) {
  const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                              std::end(kPayload));
  FakeConnectionLifecycleListener fake_connection_life_cycle_listener;
  EndpointData endpoint_data = CreateEndpointData(1);
  StartAdvertising(fake_connection_life_cycle_listener, endpoint_data);

  base::RunLoop accepted_run_loop;
  fake_connection_life_cycle_listener.accepted_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        accepted_run_loop.Quit();
      });

  FakePayloadListener fake_payload_listener;
  ClientProxy* client_proxy =
      AcceptConnection(fake_payload_listener, endpoint_data.remote_endpoint_id);
  accepted_run_loop.Run();

  base::RunLoop payload_run_loop;
  fake_payload_listener.payload_cb = base::BindLambdaForTesting(
      [&](const std::string& endpoint_id, mojom::PayloadPtr payload) {
        EXPECT_EQ(endpoint_data.remote_endpoint_id, endpoint_id);
        EXPECT_EQ(kPayloadId, payload->id);
        ASSERT_TRUE(payload->content->is_bytes());
        EXPECT_EQ(expected_payload, payload->content->get_bytes()->bytes);
        payload_run_loop.Quit();
      });

  std::string expected_payload_str(expected_payload.begin(),
                                   expected_payload.end());
  auto input_stream = std::make_unique<testing::NiceMock<MockInputStream>>();
  EXPECT_CALL(*input_stream, Read(_))
      .WillOnce(
          Return(ExceptionOr<ByteArray>(ByteArray(expected_payload_str))));
  EXPECT_CALL(*input_stream, Close());

  client_proxy->OnPayload(endpoint_data.remote_endpoint_id,
                          Payload(kPayloadId, std::move(input_stream)));
  int64_t expected_payload_size = expected_payload.size();
  client_proxy->OnPayloadProgress(
      endpoint_data.remote_endpoint_id,
      {.payload_id = kPayloadId,
       .status = PayloadProgressInfo::Status::kInProgress,
       .total_bytes = expected_payload_size,
       .bytes_transferred = expected_payload_size});
  client_proxy->OnPayloadProgress(
      endpoint_data.remote_endpoint_id,
      {.payload_id = kPayloadId,
       .status = PayloadProgressInfo::Status::kSuccess,
       .total_bytes = expected_payload_size,
       .bytes_transferred = expected_payload_size});

  payload_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, RequestConnectionV3Initiated) {
  nearby::presence::PresenceDevice presence_device(kEndpointId);
  presence_device.SetDeviceIdentityMetaData(CreateMetadata());
  PresenceDevicePtr presence_device_mojom =
      ash::nearby::presence::BuildPresenceMojomDevice(presence_device);
  EXPECT_EQ(presence_device_mojom->endpoint_id,
            presence_device.GetEndpointId());

  base::RunLoop initiated_run_loop;
  FakeConnectionListenerV3 fake_connection_listener_v3;
  fake_connection_listener_v3.initiated_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id,
                                     mojom::InitialConnectionInfoV3Ptr info) {
        EXPECT_EQ(endpoint_id, kEndpointId);
        EXPECT_EQ(info->authentication_status,
                  mojom::AuthenticationStatus::kSuccess);

        initiated_run_loop.Quit();
      });

  RequestConnectionV3(fake_connection_listener_v3,
                      std::move(presence_device_mojom),
                      AuthenticationStatus::kSuccess);
  initiated_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, RequestConnectionV3FailtoAuthenticate) {
  nearby::presence::PresenceDevice presence_device(kEndpointId);
  presence_device.SetDeviceIdentityMetaData(CreateMetadata());
  PresenceDevicePtr presence_device_mojom =
      ash::nearby::presence::BuildPresenceMojomDevice(presence_device);
  EXPECT_EQ(presence_device_mojom->endpoint_id,
            presence_device.GetEndpointId());

  base::RunLoop initiated_run_loop;
  FakeConnectionListenerV3 fake_connection_listener_v3;
  fake_connection_listener_v3.initiated_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id,
                                     mojom::InitialConnectionInfoV3Ptr info) {
        EXPECT_EQ(endpoint_id, kEndpointId);
        EXPECT_EQ(info->authentication_status,
                  mojom::AuthenticationStatus::kFailure);

        initiated_run_loop.Quit();
      });

  RequestConnectionV3(fake_connection_listener_v3,
                      std::move(presence_device_mojom),
                      AuthenticationStatus::kFailure);
  initiated_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, AcceptConnectionV3) {
  nearby::presence::PresenceDevice presence_device(kEndpointId);
  presence_device.SetDeviceIdentityMetaData(CreateMetadata());
  PresenceDevicePtr presence_device_mojom =
      ash::nearby::presence::BuildPresenceMojomDevice(presence_device);
  EXPECT_EQ(presence_device_mojom->endpoint_id,
            presence_device.GetEndpointId());

  base::RunLoop initiated_run_loop;
  FakeConnectionListenerV3 fake_connection_listener_v3;
  fake_connection_listener_v3.initiated_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id,
                                     mojom::InitialConnectionInfoV3Ptr info) {
        EXPECT_EQ(endpoint_id, kEndpointId);
        EXPECT_EQ(info->authentication_status,
                  mojom::AuthenticationStatus::kSuccess);

        initiated_run_loop.Quit();
      });

  base::RunLoop on_connection_result_run_loop;
  fake_connection_listener_v3.result_cb = base::BindLambdaForTesting(
      [&](const std::string& endpoint_id, mojom::Status status) {
        EXPECT_EQ(endpoint_id, kEndpointId);
        EXPECT_EQ(status, mojom::Status::kSuccess);

        on_connection_result_run_loop.Quit();
      });

  RequestConnectionV3(fake_connection_listener_v3,
                      presence_device_mojom.Clone(),
                      AuthenticationStatus::kSuccess);
  initiated_run_loop.Run();

  EXPECT_EQ(presence_device_mojom->endpoint_id,
            presence_device.GetEndpointId());
  FakePayloadListenerV3 fake_payload_listener_v3;
  AcceptConnectionV3(fake_payload_listener_v3,
                     std::move(presence_device_mojom));
  on_connection_result_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, RejectConnectionV3) {
  nearby::presence::PresenceDevice presence_device(kEndpointId);
  presence_device.SetDeviceIdentityMetaData(CreateMetadata());
  PresenceDevicePtr presence_device_mojom =
      ash::nearby::presence::BuildPresenceMojomDevice(presence_device);
  EXPECT_EQ(presence_device_mojom->endpoint_id,
            presence_device.GetEndpointId());

  FakeConnectionListenerV3 fake_connection_listener_v3;
  RequestConnectionV3(fake_connection_listener_v3,
                      presence_device_mojom.Clone(),
                      AuthenticationStatus::kSuccess);

  EXPECT_EQ(presence_device_mojom->endpoint_id,
            presence_device.GetEndpointId());
  RejectConnectionV3(std::move(presence_device_mojom));
}

TEST_F(NearbyConnectionsTest, DisconnectFromDeviceV3) {
  nearby::presence::PresenceDevice presence_device(kEndpointId);
  presence_device.SetDeviceIdentityMetaData(CreateMetadata());
  PresenceDevicePtr presence_device_mojom =
      ash::nearby::presence::BuildPresenceMojomDevice(presence_device);
  EXPECT_EQ(presence_device_mojom->endpoint_id,
            presence_device.GetEndpointId());

  FakeConnectionListenerV3 fake_connection_listener_v3;
  base::RunLoop initiated_run_loop;
  fake_connection_listener_v3.initiated_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id,
                                     mojom::InitialConnectionInfoV3Ptr info) {
        EXPECT_EQ(endpoint_id, kEndpointId);
        EXPECT_EQ(info->authentication_status,
                  mojom::AuthenticationStatus::kSuccess);

        initiated_run_loop.Quit();
      });

  base::RunLoop on_connection_result_run_loop;
  fake_connection_listener_v3.result_cb = base::BindLambdaForTesting(
      [&](const std::string& endpoint_id, mojom::Status status) {
        EXPECT_EQ(endpoint_id, kEndpointId);
        EXPECT_EQ(status, mojom::Status::kSuccess);

        on_connection_result_run_loop.Quit();
      });

  fake_connection_listener_v3.disconnected_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id) {
        EXPECT_EQ(endpoint_id, kEndpointId);
      });

  RequestConnectionV3(fake_connection_listener_v3,
                      presence_device_mojom.Clone(),
                      AuthenticationStatus::kSuccess);
  initiated_run_loop.Run();

  presence_device_mojom =
      ash::nearby::presence::BuildPresenceMojomDevice(presence_device);
  EXPECT_EQ(presence_device_mojom->endpoint_id,
            presence_device.GetEndpointId());
  FakePayloadListenerV3 fake_payload_listener_v3;
  ClientProxy* client_proxy = AcceptConnectionV3(fake_payload_listener_v3,
                                                 presence_device_mojom.Clone());
  on_connection_result_run_loop.Run();

  EXPECT_CALL(*service_controller_router_ptr_, DisconnectFromDeviceV3)
      .WillOnce([&client_proxy](ClientProxy* client,
                                const NearbyDevice& nearby_device,
                                ResultCallback callback) {
        client_proxy = client;

        EXPECT_EQ(kEndpointId, nearby_device.GetEndpointId());

        client_proxy->OnDisconnected(nearby_device.GetEndpointId(),
                                     /*notify=*/true);
        EXPECT_TRUE(callback);
        callback({Status::kSuccess});
      });

  presence_device_mojom =
      ash::nearby::presence::BuildPresenceMojomDevice(presence_device);
  base::RunLoop disconnect_run_loop;
  nearby_connections_->DisconnectFromDeviceV3(
      kServiceId, std::move(presence_device_mojom),
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        disconnect_run_loop.Quit();
      }));
  disconnect_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, BandwidthChangedV3CallbackSucceeds) {
  nearby::presence::PresenceDevice presence_device(kEndpointId);
  presence_device.SetDeviceIdentityMetaData(CreateMetadata());
  PresenceDevicePtr presence_device_mojom =
      ash::nearby::presence::BuildPresenceMojomDevice(presence_device);
  EXPECT_EQ(presence_device_mojom->endpoint_id,
            presence_device.GetEndpointId());

  base::RunLoop bandwidth_changed_run_loop;
  FakeConnectionListenerV3 fake_connection_listener_v3;
  fake_connection_listener_v3.bandwidth_changed_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id,
                                     mojom::BandwidthInfoPtr bandwidth_info) {
        EXPECT_EQ(endpoint_id, kEndpointId);
        EXPECT_EQ(bandwidth_info->quality, mojom::BandwidthQuality::kMedium);
        EXPECT_EQ(bandwidth_info->medium, mojom::Medium::kBluetooth);

        bandwidth_changed_run_loop.Quit();
      });

  ClientProxy* client_proxy;
  EXPECT_CALL(*service_controller_router_ptr_, RequestConnectionV3)
      .WillOnce([&](ClientProxy* client, const NearbyDevice& nearby_device,
                    v3::ConnectionRequestInfo info,
                    const ConnectionOptions& options, ResultCallback callback) {
        client_proxy = client;

        EXPECT_TRUE(options.allowed.bluetooth);
        EXPECT_EQ(kKeepAliveInterval.InMilliseconds(),
                  options.keep_alive_interval_millis);
        EXPECT_EQ(kKeepAliveTimeout.InMilliseconds(),
                  options.keep_alive_timeout_millis);
        EXPECT_EQ(kEndpointId, nearby_device.GetEndpointId());

        client_proxy->OnConnectionInitiated(
            std::string{nearby_device.GetEndpointId()},
            {.remote_endpoint_info = ByteArrayFromMojom(
                 std::vector<uint8_t>(std::begin(kRemoteEndpointInfo),
                                      std::end(kRemoteEndpointInfo))),
             .authentication_token = kAuthenticationToken,
             .raw_authentication_token = ByteArray(
                 kRawAuthenticationToken, sizeof(kRawAuthenticationToken)),
             .is_incoming_connection = false},
            options,
            ConvertConnectionListenerV3ToV1(nearby_device,
                                            std::move(info.listener)),
            kConnectionToken);

        EXPECT_TRUE(callback);
        callback({Status::kSuccess});
      });

  base::RunLoop request_connection_run_loop;
  nearby_connections_->RequestConnectionV3(
      kServiceId, std::move(presence_device_mojom),
      CreateConnectionOptions(
          std::vector<uint8_t>(std::begin(kBluetoothMacAddress),
                               std::end(kBluetoothMacAddress)),
          kKeepAliveInterval, kKeepAliveTimeout),
      fake_connection_listener_v3.receiver.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);
        request_connection_run_loop.Quit();
      }));
  request_connection_run_loop.Run();
  client_proxy->OnBandwidthChanged(kEndpointId, Medium::BLUETOOTH);
  bandwidth_changed_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, OnPayloadReceivedV3ReceiveBytesPayload) {
  nearby::presence::PresenceDevice presence_device(kEndpointId);
  presence_device.SetDeviceIdentityMetaData(CreateMetadata());
  PresenceDevicePtr presence_device_mojom =
      ash::nearby::presence::BuildPresenceMojomDevice(presence_device);
  const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                              std::end(kPayload));

  FakeConnectionListenerV3 fake_connection_listener_v3;
  RequestConnectionV3(fake_connection_listener_v3,
                      presence_device_mojom.Clone(),
                      AuthenticationStatus::kSuccess);

  FakePayloadListenerV3 fake_payload_listener_v3;
  ClientProxy* client_proxy = AcceptConnectionV3(
      fake_payload_listener_v3, std::move(presence_device_mojom));

  base::RunLoop on_payload_received_run_loop;
  fake_payload_listener_v3.payload_received_cb = base::BindLambdaForTesting(
      [&](const std::string& endpoint_id, mojom::PayloadPtr payload) {
        EXPECT_EQ(endpoint_id, kEndpointId);
        EXPECT_EQ(payload->id, kPayloadId);
        EXPECT_TRUE(payload->content->is_bytes());
        EXPECT_EQ(expected_payload, payload->content->get_bytes()->bytes);

        on_payload_received_run_loop.Quit();
      });

  client_proxy->OnPayload(
      kEndpointId, Payload(kPayloadId, ByteArrayFromMojom(expected_payload)));
  on_payload_received_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, OnPayloadReceivedV3ReceiveFilePayload) {
  nearby::presence::PresenceDevice presence_device(kEndpointId);
  presence_device.SetDeviceIdentityMetaData(CreateMetadata());
  PresenceDevicePtr presence_device_mojom =
      ash::nearby::presence::BuildPresenceMojomDevice(presence_device);
  const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                              std::end(kPayload));

  FakeConnectionListenerV3 fake_connection_listener_v3;
  RequestConnectionV3(fake_connection_listener_v3,
                      presence_device_mojom.Clone(),
                      AuthenticationStatus::kSuccess);

  FakePayloadListenerV3 fake_payload_listener_v3;
  ClientProxy* client_proxy = AcceptConnectionV3(
      fake_payload_listener_v3, std::move(presence_device_mojom));

  base::FilePath path;
  EXPECT_TRUE(base::CreateTemporaryFile(&path));
  base::File output_file(path, base::File::Flags::FLAG_CREATE_ALWAYS |
                                   base::File::Flags::FLAG_WRITE);
  EXPECT_TRUE(output_file.IsValid());
  base::File input_file(
      path, base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  EXPECT_TRUE(input_file.IsValid());

  base::RunLoop register_file_payload_file_run_loop;
  nearby_connections_->RegisterPayloadFile(
      kServiceId, kPayloadId, std::move(input_file), std::move(output_file),
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kSuccess, status);

        register_file_payload_file_run_loop.Quit();
      }));
  register_file_payload_file_run_loop.Run();

  OutputFile core_output_file(kPayloadId);
  EXPECT_TRUE(
      core_output_file.Write(ByteArrayFromMojom(expected_payload)).Ok());
  EXPECT_TRUE(core_output_file.Flush().Ok());
  EXPECT_TRUE(core_output_file.Close().Ok());

  base::RunLoop on_payload_received_run_loop;
  fake_payload_listener_v3.payload_received_cb = base::BindLambdaForTesting(
      [&](const std::string& endpoint_id, mojom::PayloadPtr payload) {
        EXPECT_EQ(endpoint_id, kEndpointId);
        EXPECT_EQ(payload->id, kPayloadId);
        EXPECT_TRUE(payload->content->is_file());

        base::File& file = payload->content->get_file()->file;
        std::vector<uint8_t> buffer(file.GetLength());
        EXPECT_TRUE(file.ReadAndCheck(/*offset=*/0, base::make_span(buffer)));
        EXPECT_EQ(expected_payload, buffer);

        on_payload_received_run_loop.Quit();
      });

  client_proxy->OnPayload(
      kEndpointId,
      Payload(kPayloadId, InputFile(kPayloadId, expected_payload.size())));
  on_payload_received_run_loop.Run();
}

TEST_F(NearbyConnectionsTest, OnPayloadTransferUpdateV3InProgress) {
  nearby::presence::PresenceDevice presence_device(kEndpointId);
  presence_device.SetDeviceIdentityMetaData(CreateMetadata());
  PresenceDevicePtr presence_device_mojom =
      ash::nearby::presence::BuildPresenceMojomDevice(presence_device);
  const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                              std::end(kPayload));

  FakeConnectionListenerV3 fake_connection_listener_v3;
  RequestConnectionV3(fake_connection_listener_v3,
                      presence_device_mojom.Clone(),
                      AuthenticationStatus::kSuccess);

  FakePayloadListenerV3 fake_payload_listener_v3;
  ClientProxy* client_proxy = AcceptConnectionV3(
      fake_payload_listener_v3, std::move(presence_device_mojom));

  PayloadProgressInfo info{
      .payload_id = kPayloadId,
      .status = PayloadProgressInfo::Status::kInProgress,
  };

  base::RunLoop on_payload_transfer_update_run_loop;
  fake_payload_listener_v3.payload_progress_cb =
      base::BindLambdaForTesting([&](const std::string& endpoint_id,
                                     mojom::PayloadTransferUpdatePtr update) {
        EXPECT_EQ(endpoint_id, kEndpointId);
        EXPECT_EQ(update->payload_id, kPayloadId);
        EXPECT_EQ(update->status, mojom::PayloadStatus::kInProgress);

        on_payload_transfer_update_run_loop.Quit();
      });

  client_proxy->OnPayloadProgress(kEndpointId, info);
  on_payload_transfer_update_run_loop.Run();
}

// TODO(b/330183112): Add test infratructure support to better handle
// verification of `Core` attributes.
TEST_F(NearbyConnectionsTest, RegisterServiceWithPresenceDeviceProvider) {
  nearby_connections_->RegisterServiceWithPresenceDeviceProvider(kServiceId);
}

}  // namespace nearby::connections
