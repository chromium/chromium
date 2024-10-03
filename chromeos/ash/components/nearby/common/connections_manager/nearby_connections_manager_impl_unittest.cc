// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager_impl.h"

#include <algorithm>
#include <memory>

#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connection_impl.h"
#include "chromeos/ash/components/nearby/presence/conversions/nearby_presence_conversions.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_connections.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_process_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "components/cross_device/nearby/nearby_features.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kServiceId[] = "NearbySharing";
const nearby::connections::mojom::Strategy kStrategy =
    nearby::connections::mojom::Strategy::kP2pPointToPoint;
const char kEndpointId[] = "ABCD";
const char kRemoteEndpointId[] = "WXYZ";
const char kEndpointInfo[] = {0x0d, 0x07, 0x07, 0x07, 0x07};
const char kRemoteEndpointInfo[] = {0x0d, 0x07, 0x06, 0x08, 0x09};
const char kAuthenticationToken[] = "authentication_token";
const char kRawAuthenticationToken[] = {0x00, 0x05, 0x04, 0x03, 0x02};
const char kBytePayload[] = {0x08, 0x09, 0x06, 0x04, 0x0f};
const char kBytePayload2[] = {0x0a, 0x0b, 0x0c, 0x0d, 0x0e};
const char kDeviceName[] = "Cris Cros's Pixel";
const std::vector<uint8_t> kDeviceId = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab,
                                        0xcd, 0xef, 0x01, 0x23, 0x45, 0x67,
                                        0x89, 0xab, 0xcd, 0xef};
const int64_t kPayloadId = 689777;
const int64_t kPayloadId2 = 777689;
const int64_t kPayloadId3 = 986777;
const uint64_t kTotalSize = 5201314;
const uint64_t kBytesTransferred = 721831;
const uint8_t kPayload[] = {0x0f, 0x0a, 0x0c, 0x0e};
const std::vector<uint8_t> kBluetoothMacAddress = {0x00, 0x00, 0xe6,
                                                   0x88, 0x64, 0x13};
const std::vector<uint8_t> kInvalidBluetoothMacAddress = {0x07, 0x07, 0x07};

// Timeout for initiating a connection to a remote device.
constexpr base::TimeDelta kInitiateNearbyConnectionTimeout = base::Seconds(60);

constexpr base::TimeDelta kConnectV3ToSuccessfulConnectionLatency =
    base::Milliseconds(123u);

void VerifyFileReadWrite(base::File& input_file, base::File& output_file) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const std::vector<uint8_t> expected_bytes(std::begin(kPayload),
                                            std::end(kPayload));
  EXPECT_TRUE(output_file.WriteAndCheck(
      /*offset=*/0, base::make_span(expected_bytes)));
  output_file.Flush();
  output_file.Close();

  std::vector<uint8_t> payload_bytes(input_file.GetLength());
  EXPECT_TRUE(input_file.ReadAndCheck(
      /*offset=*/0, base::make_span(payload_bytes)));
  EXPECT_EQ(expected_bytes, payload_bytes);
  input_file.Close();
}

base::FilePath InitializeTemporaryFile(base::File& file) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath path;
  if (!base::CreateTemporaryFile(&path)) {
    return path;
  }

  file.Initialize(path, base::File::Flags::FLAG_CREATE_ALWAYS |
                            base::File::Flags::FLAG_READ |
                            base::File::Flags::FLAG_WRITE);
  EXPECT_TRUE(file.WriteAndCheck(
      /*offset=*/0, base::make_span(kPayload, sizeof(kPayload))));
  EXPECT_TRUE(file.Flush());
  return path;
}

nearby::presence::PresenceDevice CreatePresenceDevice() {
  nearby::internal::DeviceIdentityMetaData metadata;
  metadata.set_device_type(nearby::internal::DeviceType::DEVICE_TYPE_PHONE);
  metadata.set_device_name(kDeviceName);
  metadata.set_bluetooth_mac_address(
      std::string(kBluetoothMacAddress.begin(), kBluetoothMacAddress.end()));
  metadata.set_device_id(std::string(kDeviceId.begin(), kDeviceId.end()));

  nearby::presence::PresenceDevice presence_device(kRemoteEndpointId);
  presence_device.SetDeviceIdentityMetaData(metadata);
  presence_device.AddAction(static_cast<uint32_t>(
      nearby::presence::ActionBit::kPresenceManagerAction));

  return presence_device;
}

}  // namespace

using Status = nearby::connections::mojom::Status;
using DiscoveredEndpointInfo =
    nearby::connections::mojom::DiscoveredEndpointInfo;
using ConnectionInfo = nearby::connections::mojom::ConnectionInfo;
using Medium = nearby::connections::mojom::Medium;
using BandwidthQuality = nearby::connections::mojom::BandwidthQuality;
using MediumSelection = nearby::connections::mojom::MediumSelection;
using PayloadContent = nearby::connections::mojom::PayloadContent;
using PayloadStatus = nearby::connections::mojom::PayloadStatus;
using PayloadTransferUpdate = nearby::connections::mojom::PayloadTransferUpdate;
using Payload = nearby::connections::mojom::Payload;
using BytesPayload = nearby::connections::mojom::BytesPayload;
using FilePayload = nearby::connections::mojom::FilePayload;

class MockDiscoveryListener
    : public NearbyConnectionsManager::DiscoveryListener {
 public:
  MOCK_METHOD(void,
              OnEndpointDiscovered,
              (const std::string& endpoint_id,
               const std::vector<uint8_t>& endpoint_info),
              (override));
  MOCK_METHOD(void,
              OnEndpointLost,
              (const std::string& endpoint_id),
              (override));
};

class MockIncomingConnectionListener
    : public NearbyConnectionsManager::IncomingConnectionListener {
 public:
  MOCK_METHOD(void,
              OnIncomingConnectionInitiated,
              (const std::string& endpoint_id,
               const std::vector<uint8_t>& endpoint_info),
              (override));

  MOCK_METHOD(void,
              OnIncomingConnectionAccepted,
              (const std::string& endpoint_id,
               const std::vector<uint8_t>& endpoint_info,
               NearbyConnection* connection),
              (override));
};

class MockPayloadStatusListener
    : public NearbyConnectionsManager::PayloadStatusListener {
 public:
  MOCK_METHOD(void,
              OnStatusUpdate,
              (PayloadTransferUpdatePtr update,
               std::optional<Medium> upgraded_medium),
              (override));
};

class MockBandwidthUpgradeListener
    : public NearbyConnectionsManager::BandwidthUpgradeListener {
 public:
  MOCK_METHOD(void,
              OnInitialMedium,
              (const std::string& endpoint_id, const Medium medium),
              (override));

  MOCK_METHOD(void,
              OnBandwidthUpgrade,
              (const std::string& endpoint_id, const Medium medium),
              (override));

  MOCK_METHOD(void,
              OnBandwidthUpgradeV3,
              (nearby::presence::PresenceDevice, const Medium medium),
              (override));

  base::WeakPtr<MockBandwidthUpgradeListener> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockBandwidthUpgradeListener> weak_ptr_factory_{this};
};

class NearbyConnectionsManagerImplTest : public testing::Test {
 public:
  void SetUp() override {
    nearby_connections_manager_ =
        std::make_unique<NearbyConnectionsManagerImpl>(&nearby_process_manager_,
                                                       kServiceId);
    scoped_feature_list_.InitAndEnableFeature(features::kNearbySharingWebRtc);
    histogram_tester_ = std::make_unique<base::HistogramTester>();

    EXPECT_CALL(nearby_process_manager_, GetNearbyProcessReference)
        .WillRepeatedly([&](ash::nearby::NearbyProcessManager::
                                NearbyProcessStoppedCallback) {
          auto mock_reference_ptr =
              std::make_unique<ash::nearby::MockNearbyProcessManager::
                                   MockNearbyProcessReference>();

          EXPECT_CALL(*(mock_reference_ptr.get()), GetNearbyConnections)
              .WillRepeatedly(
                  testing::ReturnRef(nearby_connections_.shared_remote()));

          return mock_reference_ptr;
        });
  }

 protected:
  void StartDiscovery(
      mojo::Remote<EndpointDiscoveryListener>& listener_remote,
      testing::NiceMock<MockDiscoveryListener>& discovery_listener) {
    StartDiscovery(listener_remote, default_data_usage_, discovery_listener);
  }

  void StartDiscovery(
      mojo::Remote<EndpointDiscoveryListener>& listener_remote,
      NearbyConnectionsManager::DataUsage data_usage,
      testing::NiceMock<MockDiscoveryListener>& discovery_listener) {
    EXPECT_CALL(nearby_connections_, StartDiscovery)
        .WillOnce([&listener_remote, this](
                      const std::string& service_id,
                      DiscoveryOptionsPtr options,
                      mojo::PendingRemote<EndpointDiscoveryListener> listener,
                      NearbyConnectionsMojom::StartDiscoveryCallback callback) {
          EXPECT_EQ(kServiceId, service_id);
          EXPECT_EQ(kStrategy, options->strategy);
          EXPECT_TRUE(options->allowed_mediums->bluetooth);
          EXPECT_TRUE(options->allowed_mediums->ble);
          EXPECT_EQ(should_use_web_rtc_, options->allowed_mediums->web_rtc);
          EXPECT_EQ(should_use_wifilan_, options->allowed_mediums->wifi_lan);
          EXPECT_EQ(should_use_wifidirect_,
                    options->allowed_mediums->wifi_direct);
          EXPECT_EQ(
              device::BluetoothUUID("0000fef3-0000-1000-8000-00805f9b34fb"),
              options->fast_advertisement_service_uuid);

          listener_remote.Bind(std::move(listener));
          std::move(callback).Run(Status::kSuccess);
        });

    base::RunLoop run_loop;
    NearbyConnectionsManager::ConnectionsCallback callback =
        base::BindLambdaForTesting([&run_loop](Status status) {
          EXPECT_EQ(status, Status::kSuccess);
          run_loop.Quit();
        });

    nearby_connections_manager_->StartDiscovery(&discovery_listener, data_usage,
                                                std::move(callback));

    run_loop.Run();
  }

  void StartAdvertising(
      mojo::Remote<ConnectionLifecycleListener>& listener_remote,
      testing::NiceMock<MockIncomingConnectionListener>&
          incoming_connection_listener) {
    const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                   std::end(kEndpointInfo));
    EXPECT_CALL(nearby_connections_, StartAdvertising)
        .WillOnce(
            [&](const std::string& service_id,
                const std::vector<uint8_t>& endpoint_info,
                AdvertisingOptionsPtr options,
                mojo::PendingRemote<ConnectionLifecycleListener> listener,
                NearbyConnectionsMojom::StartAdvertisingCallback callback) {
              EXPECT_EQ(kServiceId, service_id);
              EXPECT_EQ(local_endpoint_info, endpoint_info);
              EXPECT_EQ(kStrategy, options->strategy);
              EXPECT_TRUE(options->enforce_topology_constraints);

              listener_remote.Bind(std::move(listener));
              std::move(callback).Run(Status::kSuccess);
            });
    base::RunLoop run_loop;
    NearbyConnectionsManager::ConnectionsCallback callback =
        base::BindLambdaForTesting([&run_loop](Status status) {
          EXPECT_EQ(status, Status::kSuccess);
          run_loop.Quit();
        });
    nearby_connections_manager_->StartAdvertising(
        local_endpoint_info, &incoming_connection_listener,
        NearbyConnectionsManager::PowerLevel::kHighPower,
        NearbyConnectionsManager::DataUsage::kOnline, std::move(callback));
    run_loop.Run();
  }

  enum class ConnectionResponse { kAccepted, kRejceted, kDisconnected };

  NearbyConnection* Connect(
      mojo::Remote<ConnectionLifecycleListener>& connection_listener_remote,
      mojo::Remote<PayloadListener>& payload_listener_remote,
      ConnectionResponse connection_response) {
    const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                   std::end(kEndpointInfo));
    const std::vector<uint8_t> remote_endpoint_info(
        std::begin(kRemoteEndpointInfo), std::end(kRemoteEndpointInfo));
    const std::vector<uint8_t> raw_authentication_token(
        std::begin(kRawAuthenticationToken), std::end(kRawAuthenticationToken));

    base::RunLoop request_connection_run_loop;
    EXPECT_CALL(nearby_connections_, RequestConnection)
        .WillOnce(
            [&](const std::string& service_id,
                const std::vector<uint8_t>& endpoint_info,
                const std::string& endpoint_id, ConnectionOptionsPtr options,
                mojo::PendingRemote<ConnectionLifecycleListener> listener,
                NearbyConnectionsMojom::RequestConnectionCallback callback) {
              EXPECT_EQ(kServiceId, service_id);
              EXPECT_EQ(local_endpoint_info, endpoint_info);
              EXPECT_EQ(kRemoteEndpointId, endpoint_id);

              connection_listener_remote.Bind(std::move(listener));
              std::move(callback).Run(Status::kSuccess);
              request_connection_run_loop.Quit();
            });

    base::RunLoop run_loop;
    NearbyConnection* nearby_connection;
    nearby_connections_manager_->Connect(
        local_endpoint_info, kRemoteEndpointId,
        /*bluetooth_mac_address=*/std::nullopt,
        NearbyConnectionsManager::DataUsage::kOffline,
        base::BindLambdaForTesting([&](NearbyConnection* connection) {
          nearby_connection = connection;
        }));

    request_connection_run_loop.Run();

    EXPECT_CALL(nearby_connections_, AcceptConnection)
        .WillOnce(
            [&](const std::string& service_id, const std::string& endpoint_id,
                mojo::PendingRemote<PayloadListener> listener,
                NearbyConnectionsMojom::AcceptConnectionCallback callback) {
              EXPECT_EQ(kServiceId, service_id);
              EXPECT_EQ(kRemoteEndpointId, endpoint_id);

              payload_listener_remote.Bind(std::move(listener));
              std::move(callback).Run(Status::kSuccess);
              run_loop.Quit();
            });

    connection_listener_remote->OnConnectionInitiated(
        kRemoteEndpointId,
        ConnectionInfo::New(kAuthenticationToken, raw_authentication_token,
                            remote_endpoint_info,
                            /*is_incoming_connection=*/false));

    switch (connection_response) {
      case ConnectionResponse::kAccepted:
        connection_listener_remote->OnConnectionAccepted(kRemoteEndpointId);
        break;
      case ConnectionResponse::kRejceted:
        connection_listener_remote->OnConnectionRejected(
            kRemoteEndpointId, Status::kConnectionRejected);
        break;
      case ConnectionResponse::kDisconnected:
        connection_listener_remote->OnDisconnected(kRemoteEndpointId);
        break;
    }
    run_loop.Run();

    return nearby_connection;
  }

  NearbyConnection* OnIncomingConnection(
      mojo::Remote<ConnectionLifecycleListener>& connection_listener_remote,
      testing::NiceMock<MockIncomingConnectionListener>&
          incoming_connection_listener,
      mojo::Remote<PayloadListener>& payload_listener_remote) {
    base::RunLoop accept_run_loop;
    EXPECT_CALL(incoming_connection_listener,
                OnIncomingConnectionInitiated(testing::_, testing::_));
    EXPECT_CALL(nearby_connections_, AcceptConnection)
        .WillOnce(
            [&](const std::string& service_id, const std::string& endpoint_id,
                mojo::PendingRemote<PayloadListener> listener,
                NearbyConnectionsMojom::AcceptConnectionCallback callback) {
              EXPECT_EQ(kServiceId, service_id);
              EXPECT_EQ(kRemoteEndpointId, endpoint_id);

              payload_listener_remote.Bind(std::move(listener));
              std::move(callback).Run(Status::kSuccess);
              accept_run_loop.Quit();
            });

    const std::vector<uint8_t> remote_endpoint_info(
        std::begin(kRemoteEndpointInfo), std::end(kRemoteEndpointInfo));
    const std::vector<uint8_t> raw_authentication_token(
        std::begin(kRawAuthenticationToken), std::end(kRawAuthenticationToken));

    connection_listener_remote->OnConnectionInitiated(
        kRemoteEndpointId,
        ConnectionInfo::New(kAuthenticationToken, raw_authentication_token,
                            remote_endpoint_info,
                            /*is_incoming_connection=*/true));
    accept_run_loop.Run();

    NearbyConnection* nearby_connection;
    base::RunLoop incoming_connection_run_loop;
    EXPECT_CALL(
        incoming_connection_listener,
        OnIncomingConnectionAccepted(testing::_, testing::_, testing::_))
        .WillOnce([&](const std::string&, const std::vector<uint8_t>&,
                      NearbyConnection* connection) {
          nearby_connection = connection;
          incoming_connection_run_loop.Quit();
        });

    connection_listener_remote->OnConnectionAccepted(kRemoteEndpointId);
    incoming_connection_run_loop.Run();

    EXPECT_EQ(raw_authentication_token,
              nearby_connections_manager_->GetRawAuthenticationToken(
                  kRemoteEndpointId));

    return nearby_connection;
  }

  void SendPayload(
      int64_t payload_id,
      testing::NiceMock<MockPayloadStatusListener>& payload_listener) {
    const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                                std::end(kPayload));

    base::File file;
    InitializeTemporaryFile(file);

    base::RunLoop run_loop;
    EXPECT_CALL(nearby_connections_, SendPayload)
        .WillOnce([&](const std::string& service_id,
                      const std::vector<std::string>& endpoint_ids,
                      PayloadPtr payload,
                      NearbyConnectionsMojom::SendPayloadCallback callback) {
          ASSERT_EQ(1u, endpoint_ids.size());
          EXPECT_EQ(kServiceId, service_id);
          EXPECT_EQ(kRemoteEndpointId, endpoint_ids.front());
          ASSERT_TRUE(payload);
          EXPECT_EQ(payload_id, payload->id);
          ASSERT_EQ(PayloadContent::Tag::kFile, payload->content->which());

          base::ScopedAllowBlockingForTesting allow_blocking;
          base::File file = std::move(payload->content->get_file()->file);
          std::vector<uint8_t> payload_bytes(file.GetLength());
          EXPECT_TRUE(file.ReadAndCheck(
              /*offset=*/0, base::make_span(payload_bytes)));
          EXPECT_EQ(expected_payload, payload_bytes);

          std::move(callback).Run(Status::kSuccess);
          run_loop.Quit();
        });

    nearby_connections_manager_->Send(
        kRemoteEndpointId,
        Payload::New(payload_id, PayloadContent::NewFile(
                                     FilePayload::New(std::move(file)))),
        payload_listener.GetWeakPtr());
    run_loop.Run();
  }

  void ConnectV3(
      nearby::presence::PresenceDevice remote_presence_device,
      mojo::Remote<ConnectionListenerV3>& connection_listener_v3_remote,
      mojo::Remote<PayloadListenerV3>& payload_listener_v3_remote,
      nearby::connections::mojom::InitialConnectionInfoV3Ptr info_v3,
      Status on_connection_result_status) {
    ash::nearby::presence::mojom::PresenceDevicePtr presence_device_mojom =
        ash::nearby::presence::BuildPresenceMojomDevice(remote_presence_device);

    base::RunLoop request_connection_run_loop;
    EXPECT_CALL(nearby_connections_, RequestConnectionV3)
        .WillOnce(
            [&](const std::string& service_id,
                ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
                ConnectionOptionsPtr options,
                mojo::PendingRemote<ConnectionListenerV3> listener,
                NearbyConnectionsMojom::RequestConnectionV3Callback callback) {
              EXPECT_EQ(kServiceId, service_id);
              EXPECT_EQ(remote_device->endpoint_id, kRemoteEndpointId);

              connection_listener_v3_remote.Bind(std::move(listener));
              std::move(callback).Run(Status::kSuccess);
              request_connection_run_loop.Quit();
            });

    base::RunLoop accept_or_reject_run_loop;
    NearbyConnection* nearby_connection;
    nearby_connections_manager_->ConnectV3(
        remote_presence_device, NearbyConnectionsManager::DataUsage::kOffline,
        base::BindLambdaForTesting([&](NearbyConnection* connection) {
          nearby_connection = connection;

          if (on_connection_result_status == Status::kSuccess) {
            EXPECT_TRUE(nearby_connection);
          } else {
            EXPECT_FALSE(nearby_connection);
          }
        }));
    task_environment_.FastForwardBy(kConnectV3ToSuccessfulConnectionLatency);

    request_connection_run_loop.Run();
    if (info_v3->authentication_status ==
        nearby::connections::mojom::AuthenticationStatus::kSuccess) {
      EXPECT_CALL(nearby_connections_, AcceptConnectionV3)
          .WillOnce(
              [&](const std::string& service_id,
                  ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
                  mojo::PendingRemote<PayloadListenerV3> listener,
                  NearbyConnectionsMojom::AcceptConnectionV3Callback callback) {
                EXPECT_EQ(kServiceId, service_id);
                EXPECT_EQ(remote_device->endpoint_id, kRemoteEndpointId);

                payload_listener_v3_remote.Bind(std::move(listener));
                std::move(callback).Run(Status::kSuccess);

                accept_or_reject_run_loop.Quit();
              });
    } else {
      EXPECT_CALL(nearby_connections_, RejectConnectionV3)
          .WillOnce(
              [&](const std::string& service_id,
                  ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
                  NearbyConnectionsMojom::RejectConnectionV3Callback callback) {
                EXPECT_EQ(kServiceId, service_id);
                EXPECT_EQ(remote_device->endpoint_id, kRemoteEndpointId);

                std::move(callback).Run(Status::kSuccess);

                accept_or_reject_run_loop.Quit();
              });
    }

    connection_listener_v3_remote->OnConnectionInitiatedV3(
        presence_device_mojom->endpoint_id, std::move(info_v3));
    connection_listener_v3_remote->OnConnectionResultV3(
        presence_device_mojom->endpoint_id, on_connection_result_status);
    accept_or_reject_run_loop.Run();
  }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  bool should_use_web_rtc_ = true;
  bool should_use_wifilan_ = false;
  bool should_use_wifidirect_ = false;
  NearbyConnectionsManager::DataUsage default_data_usage_ =
      NearbyConnectionsManager::DataUsage::kWifiOnly;
  std::unique_ptr<net::test::MockNetworkChangeNotifier> network_notifier_ =
      net::test::MockNetworkChangeNotifier::Create();
  base::ScopedDisallowBlocking disallow_blocking_;
  testing::NiceMock<ash::nearby::MockNearbyConnections> nearby_connections_;
  testing::NiceMock<ash::nearby::MockNearbyProcessManager>
      nearby_process_manager_;
  std::unique_ptr<NearbyConnectionsManagerImpl> nearby_connections_manager_;
};

TEST_F(NearbyConnectionsManagerImplTest, DiscoveryFlow) {
  const std::vector<uint8_t> endpoint_info(std::begin(kEndpointInfo),
                                           std::end(kEndpointInfo));

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(listener_remote, discovery_listener);

  // Invoking OnEndpointFound over remote will invoke OnEndpointDiscovered.
  base::RunLoop discovered_run_loop;
  EXPECT_CALL(discovery_listener,
              OnEndpointDiscovered(testing::Eq(kEndpointId),
                                   testing::Eq(endpoint_info)))
      .WillOnce([&discovered_run_loop]() { discovered_run_loop.Quit(); });
  listener_remote->OnEndpointFound(
      kEndpointId, DiscoveredEndpointInfo::New(endpoint_info, kServiceId));
  discovered_run_loop.Run();

  // Invoking OnEndpointFound over remote on same endpointId will do nothing.
  EXPECT_CALL(discovery_listener, OnEndpointDiscovered(testing::_, testing::_))
      .Times(0);
  listener_remote->OnEndpointFound(
      kEndpointId, DiscoveredEndpointInfo::New(endpoint_info, kServiceId));

  // Invoking OnEndpointLost over remote will invoke OnEndpointLost.
  base::RunLoop lost_run_loop;
  EXPECT_CALL(discovery_listener, OnEndpointLost(testing::Eq(kEndpointId)))
      .WillOnce([&lost_run_loop]() { lost_run_loop.Quit(); });
  listener_remote->OnEndpointLost(kEndpointId);
  lost_run_loop.Run();

  // Invoking OnEndpointLost over remote on same endpointId will do nothing.
  EXPECT_CALL(discovery_listener, OnEndpointLost(testing::_)).Times(0);
  listener_remote->OnEndpointLost(kEndpointId);

  // After OnEndpointLost the same endpointId can be discovered again.
  base::RunLoop discovered_run_loop_2;
  EXPECT_CALL(discovery_listener,
              OnEndpointDiscovered(testing::Eq(kEndpointId),
                                   testing::Eq(endpoint_info)))
      .WillOnce([&discovered_run_loop_2]() { discovered_run_loop_2.Quit(); });
  listener_remote->OnEndpointFound(
      kEndpointId, DiscoveredEndpointInfo::New(endpoint_info, kServiceId));
  discovered_run_loop_2.Run();

  // Stop discvoery will call through mojo.
  base::RunLoop stop_discovery_run_loop;
  EXPECT_CALL(nearby_connections_, StopDiscovery)
      .WillOnce([&stop_discovery_run_loop](
                    const std::string& service_id,
                    NearbyConnectionsMojom::StopDiscoveryCallback callback) {
        EXPECT_EQ(kServiceId, service_id);
        std::move(callback).Run(Status::kSuccess);
        stop_discovery_run_loop.Quit();
      });
  nearby_connections_manager_->StopDiscovery();
  stop_discovery_run_loop.Run();

  // StartDiscovery again will succeed.
  listener_remote.reset();
  StartDiscovery(listener_remote, discovery_listener);

  // Same endpointId can be discovered again.
  base::RunLoop discovered_run_loop_3;
  EXPECT_CALL(discovery_listener,
              OnEndpointDiscovered(testing::Eq(kEndpointId),
                                   testing::Eq(endpoint_info)))
      .WillOnce([&discovered_run_loop_3]() { discovered_run_loop_3.Quit(); });
  listener_remote->OnEndpointFound(
      kEndpointId, DiscoveredEndpointInfo::New(endpoint_info, kServiceId));
  discovered_run_loop_3.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, DiscoveryProcessStopped) {
  const std::vector<uint8_t> endpoint_info(std::begin(kEndpointInfo),
                                           std::end(kEndpointInfo));

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(listener_remote, discovery_listener);

  EXPECT_CALL(nearby_connections_, StopAllEndpoints).Times(0);
  nearby_connections_manager_->OnNearbyProcessStopped(
      ash::nearby::NearbyProcessManager::NearbyProcessShutdownReason::kNormal);

  // Invoking OnEndpointFound will do nothing.
  EXPECT_CALL(discovery_listener, OnEndpointDiscovered(testing::_, testing::_))
      .Times(0);
  listener_remote->OnEndpointFound(
      kEndpointId, DiscoveredEndpointInfo::New(endpoint_info, kServiceId));
}

TEST_F(NearbyConnectionsManagerImplTest, StopDiscoveryBeforeStart) {
  EXPECT_CALL(nearby_connections_, StopDiscovery).Times(0);
  nearby_connections_manager_->StopDiscovery();
}

/******************************************************************************/
// Begin: NearbyConnectionsManagerImplTestConnectionMediums
/******************************************************************************/
using ConnectionMediumsTestParam =
    std::tuple<NearbyConnectionsManager::DataUsage,
               net::NetworkChangeNotifier::ConnectionType,
               bool,
               bool,
               bool>;
class NearbyConnectionsManagerImplTestConnectionMediums
    : public NearbyConnectionsManagerImplTest,
      public testing::WithParamInterface<ConnectionMediumsTestParam> {};

TEST_P(NearbyConnectionsManagerImplTestConnectionMediums,
       RequestConnection_MediumSelection) {
  const ConnectionMediumsTestParam& param = GetParam();
  NearbyConnectionsManager::DataUsage data_usage = std::get<0>(param);
  net::NetworkChangeNotifier::ConnectionType connection_type =
      std::get<1>(param);
  bool is_webrtc_enabled = std::get<2>(GetParam());
  bool is_wifilan_enabled = std::get<3>(GetParam());
  bool is_wifidirect_enabled = std::get<4>(GetParam());

  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  if (is_webrtc_enabled) {
    enabled_features.push_back(features::kNearbySharingWebRtc);
  } else {
    disabled_features.push_back(features::kNearbySharingWebRtc);
  }
  if (is_wifilan_enabled) {
    enabled_features.push_back(features::kNearbySharingWifiLan);
  } else {
    disabled_features.push_back(features::kNearbySharingWifiLan);
  }
  if (is_wifidirect_enabled) {
    enabled_features.push_back(features::kNearbySharingWifiDirect);
  } else {
    disabled_features.push_back(features::kNearbySharingWifiDirect);
  }
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

  network_notifier_->SetConnectionType(connection_type);
  network_notifier_->SetUseDefaultConnectionCostImplementation(true);
  bool should_use_internet =
      data_usage != NearbyConnectionsManager::DataUsage::kOffline &&
      connection_type != net::NetworkChangeNotifier::CONNECTION_NONE &&
      (data_usage != NearbyConnectionsManager::DataUsage::kWifiOnly ||
       (net::NetworkChangeNotifier::GetConnectionCost() !=
        net::NetworkChangeNotifier::CONNECTION_COST_METERED));
  bool is_connection_wifi_or_ethernet =
      connection_type == net::NetworkChangeNotifier::CONNECTION_WIFI ||
      connection_type == net::NetworkChangeNotifier::CONNECTION_ETHERNET;
  should_use_web_rtc_ = is_webrtc_enabled && should_use_internet;
  should_use_wifilan_ = is_wifilan_enabled && should_use_internet &&
                        is_connection_wifi_or_ethernet;
  should_use_wifidirect_ = is_wifidirect_enabled;

  // TODO(crbug.com/1129069): Update when WiFi LAN is supported.
  auto expected_mediums = MediumSelection::New(
      /*bluetooth=*/true,
      /*ble=*/false,
      /*web_rtc=*/should_use_web_rtc_,
      /*wifi_lan=*/should_use_wifilan_,
      /*wifi_direct=*/should_use_wifidirect_);

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, data_usage, discovery_listener);

  base::RunLoop run_loop;
  const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                 std::end(kEndpointInfo));
  EXPECT_CALL(nearby_connections_, RequestConnection)
      .WillOnce(
          [&](const std::string& service_id,
              const std::vector<uint8_t>& endpoint_info,
              const std::string& endpoint_id, ConnectionOptionsPtr options,
              mojo::PendingRemote<ConnectionLifecycleListener> listener,
              NearbyConnectionsMojom::RequestConnectionCallback callback) {
            EXPECT_EQ(kServiceId, service_id);
            EXPECT_EQ(local_endpoint_info, endpoint_info);
            EXPECT_EQ(kRemoteEndpointId, endpoint_id);
            EXPECT_EQ(expected_mediums, options->allowed_mediums);
            std::move(callback).Run(Status::kSuccess);
            run_loop.Quit();
          });

  nearby_connections_manager_->Connect(local_endpoint_info, kRemoteEndpointId,
                                       /*bluetooth_mac_address=*/std::nullopt,
                                       data_usage, base::DoNothing());

  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(
    NearbyConnectionsManagerImplTestConnectionMediums,
    NearbyConnectionsManagerImplTestConnectionMediums,
    testing::Combine(
        testing::Values(NearbyConnectionsManager::DataUsage::kWifiOnly,
                        NearbyConnectionsManager::DataUsage::kOffline,
                        NearbyConnectionsManager::DataUsage::kOnline),
        testing::Values(net::NetworkChangeNotifier::CONNECTION_NONE,
                        net::NetworkChangeNotifier::CONNECTION_WIFI,
                        net::NetworkChangeNotifier::CONNECTION_3G),
        testing::Bool(),
        testing::Bool(),
        testing::Bool()));
/******************************************************************************/
// End: NearbyConnectionsManagerImplTestConnectionMediums
/******************************************************************************/

/******************************************************************************/
// Begin: NearbyConnectionsManagerImplTestConnectionBluetoothMacAddress
/******************************************************************************/
struct ConnectionBluetoothMacAddressTestData {
  std::optional<std::vector<uint8_t>> bluetooth_mac_address;
  std::optional<std::vector<uint8_t>> expected_bluetooth_mac_address;
} kConnectionBluetoothMacAddressTestData[] = {
    {std::make_optional(kBluetoothMacAddress),
     std::make_optional(kBluetoothMacAddress)},
    {std::make_optional(kInvalidBluetoothMacAddress), std::nullopt},
    {std::nullopt, std::nullopt}};

class NearbyConnectionsManagerImplTestConnectionBluetoothMacAddress
    : public NearbyConnectionsManagerImplTest,
      public testing::WithParamInterface<
          ConnectionBluetoothMacAddressTestData> {};

TEST_P(NearbyConnectionsManagerImplTestConnectionBluetoothMacAddress,
       RequestConnection_BluetoothMacAddress) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  base::RunLoop run_loop;
  const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                 std::end(kEndpointInfo));
  EXPECT_CALL(nearby_connections_, RequestConnection)
      .WillOnce(
          [&](const std::string& service_id,
              const std::vector<uint8_t>& endpoint_info,
              const std::string& endpoint_id, ConnectionOptionsPtr options,
              mojo::PendingRemote<ConnectionLifecycleListener> listener,
              NearbyConnectionsMojom::RequestConnectionCallback callback) {
            EXPECT_EQ(kServiceId, service_id);
            EXPECT_EQ(local_endpoint_info, endpoint_info);
            EXPECT_EQ(kRemoteEndpointId, endpoint_id);
            EXPECT_EQ(GetParam().expected_bluetooth_mac_address,
                      options->remote_bluetooth_mac_address);
            std::move(callback).Run(Status::kSuccess);
            run_loop.Quit();
          });

  nearby_connections_manager_->Connect(
      local_endpoint_info, kRemoteEndpointId, GetParam().bluetooth_mac_address,
      NearbyConnectionsManager::DataUsage::kOffline, base::DoNothing());

  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(
    NearbyConnectionsManagerImplTestConnectionBluetoothMacAddress,
    NearbyConnectionsManagerImplTestConnectionBluetoothMacAddress,
    testing::ValuesIn(kConnectionBluetoothMacAddressTestData));
/******************************************************************************/
// End: NearbyConnectionsManagerImplTestConnectionBluetoothMacAddress
/******************************************************************************/

TEST_F(NearbyConnectionsManagerImplTest, ConnectRejected) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kRejceted);
  EXPECT_FALSE(nearby_connection);
  EXPECT_FALSE(nearby_connections_manager_->GetRawAuthenticationToken(
      kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectDisconnected) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kDisconnected);
  EXPECT_FALSE(nearby_connection);
  EXPECT_FALSE(nearby_connections_manager_->GetRawAuthenticationToken(
      kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectAccepted) {
  const std::vector<uint8_t> raw_authentication_token(
      std::begin(kRawAuthenticationToken), std::end(kRawAuthenticationToken));

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  EXPECT_TRUE(nearby_connection);
  EXPECT_EQ(raw_authentication_token,
            nearby_connections_manager_->GetRawAuthenticationToken(
                kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectReadBeforeAppend) {
  const std::vector<uint8_t> byte_payload(std::begin(kBytePayload),
                                          std::end(kBytePayload));

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Read before message is appended should also succeed.
  base::RunLoop read_run_loop;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](std::optional<std::vector<uint8_t>> bytes) {
        EXPECT_EQ(byte_payload, bytes);
        read_run_loop.Quit();
      }));

  payload_listener_remote->OnPayloadReceived(
      kRemoteEndpointId,
      Payload::New(kPayloadId,
                   PayloadContent::NewBytes(BytesPayload::New(byte_payload))));
  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
  read_run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectReadAfterAppend) {
  const std::vector<uint8_t> byte_payload(std::begin(kBytePayload),
                                          std::end(kBytePayload));
  const std::vector<uint8_t> byte_payload_2(std::begin(kBytePayload2),
                                            std::end(kBytePayload2));

  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Read after message is appended should succeed.
  payload_listener_remote->OnPayloadReceived(
      kRemoteEndpointId,
      Payload::New(kPayloadId,
                   PayloadContent::NewBytes(BytesPayload::New(byte_payload))));
  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
  payload_listener_remote->OnPayloadReceived(
      kRemoteEndpointId,
      Payload::New(kPayloadId2, PayloadContent::NewBytes(
                                    BytesPayload::New(byte_payload_2))));
  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId2, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));

  base::RunLoop read_run_loop;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](std::optional<std::vector<uint8_t>> bytes) {
        EXPECT_EQ(byte_payload, bytes);
        read_run_loop.Quit();
      }));
  read_run_loop.Run();

  base::RunLoop read_run_loop_2;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](std::optional<std::vector<uint8_t>> bytes) {
        EXPECT_EQ(byte_payload_2, bytes);
        read_run_loop_2.Quit();
      }));
  read_run_loop_2.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectWrite) {
  const std::vector<uint8_t> byte_payload(std::begin(kBytePayload),
                                          std::end(kBytePayload));
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  base::RunLoop run_loop;
  EXPECT_CALL(nearby_connections_, SendPayload)
      .WillOnce([&](const std::string& service_id,
                    const std::vector<std::string>& endpoint_ids,
                    PayloadPtr payload,
                    NearbyConnectionsMojom::SendPayloadCallback callback) {
        EXPECT_EQ(kServiceId, service_id);
        ASSERT_EQ(1u, endpoint_ids.size());
        EXPECT_EQ(kRemoteEndpointId, endpoint_ids.front());
        ASSERT_TRUE(payload);
        ASSERT_EQ(PayloadContent::Tag::kBytes, payload->content->which());
        EXPECT_EQ(byte_payload, payload->content->get_bytes()->bytes);

        std::move(callback).Run(Status::kSuccess);
        run_loop.Quit();
      });

  nearby_connection->Write(byte_payload);
  run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectClosed) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Close should invoke disconnection callback and read callback.
  base::RunLoop close_run_loop;
  nearby_connection->SetDisconnectionListener(
      base::BindLambdaForTesting([&]() { close_run_loop.Quit(); }));
  base::RunLoop read_run_loop_3;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](std::optional<std::vector<uint8_t>> bytes) {
        EXPECT_FALSE(bytes);
        read_run_loop_3.Quit();
      }));

  base::RunLoop disconnect_run_loop;
  EXPECT_CALL(nearby_connections_, DisconnectFromEndpoint)
      .WillOnce(
          [&](const std::string& service_id, const std::string& endpoint_id,
              NearbyConnectionsMojom::DisconnectFromEndpointCallback callback) {
            EXPECT_EQ(kServiceId, service_id);
            EXPECT_EQ(kRemoteEndpointId, endpoint_id);
            std::move(callback).Run(Status::kSuccess);
            disconnect_run_loop.Quit();
          });
  nearby_connection->Close();
  close_run_loop.Run();
  read_run_loop_3.Run();
  disconnect_run_loop.Run();

  EXPECT_FALSE(nearby_connections_manager_->GetRawAuthenticationToken(
      kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectClosedByRemote) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Remote closing should invoke disconnection callback and read callback.
  base::RunLoop close_run_loop;
  nearby_connection->SetDisconnectionListener(
      base::BindLambdaForTesting([&]() { close_run_loop.Quit(); }));
  base::RunLoop read_run_loop;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](std::optional<std::vector<uint8_t>> bytes) {
        EXPECT_FALSE(bytes);
        read_run_loop.Quit();
      }));

  connection_listener_remote->OnDisconnected(kRemoteEndpointId);
  close_run_loop.Run();
  read_run_loop.Run();

  EXPECT_FALSE(nearby_connections_manager_->GetRawAuthenticationToken(
      kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectClosedByClient) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Remote closing should invoke disconnection callback and read callback.
  base::RunLoop close_run_loop;
  nearby_connection->SetDisconnectionListener(
      base::BindLambdaForTesting([&]() { close_run_loop.Quit(); }));
  base::RunLoop read_run_loop;
  nearby_connection->Read(base::BindLambdaForTesting(
      [&](std::optional<std::vector<uint8_t>> bytes) {
        EXPECT_FALSE(bytes);
        read_run_loop.Quit();
      }));

  base::RunLoop disconnect_run_loop;
  EXPECT_CALL(nearby_connections_, DisconnectFromEndpoint)
      .WillOnce(
          [&](const std::string& service_id, const std::string& endpoint_id,
              NearbyConnectionsMojom::DisconnectFromEndpointCallback callback) {
            EXPECT_EQ(kServiceId, service_id);
            EXPECT_EQ(kRemoteEndpointId, endpoint_id);
            std::move(callback).Run(Status::kSuccess);
            disconnect_run_loop.Quit();
          });
  nearby_connections_manager_->Disconnect(kRemoteEndpointId);
  close_run_loop.Run();
  read_run_loop.Run();
  disconnect_run_loop.Run();

  EXPECT_FALSE(nearby_connections_manager_->GetRawAuthenticationToken(
      kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectSendPayload) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  Connect(connection_listener_remote, payload_listener_remote,
          ConnectionResponse::kAccepted);

  testing::NiceMock<MockPayloadStatusListener> payload_listener;
  SendPayload(kPayloadId, payload_listener);

  auto expected_update = PayloadTransferUpdate::New(
      kPayloadId, PayloadStatus::kInProgress, kTotalSize, kBytesTransferred);
  base::RunLoop payload_run_loop;
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_, testing::_))
      .WillOnce([&](MockPayloadStatusListener::PayloadTransferUpdatePtr update,
                    std::optional<Medium> upgraded_medium) {
        EXPECT_EQ(expected_update, update);
        EXPECT_EQ(std::nullopt, upgraded_medium);
        payload_run_loop.Quit();
      });

  payload_listener_remote->OnPayloadTransferUpdate(kRemoteEndpointId,
                                                   expected_update.Clone());
  payload_run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectCancelPayload) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  Connect(connection_listener_remote, payload_listener_remote,
          ConnectionResponse::kAccepted);

  testing::NiceMock<MockPayloadStatusListener> payload_listener;
  SendPayload(kPayloadId, payload_listener);

  base::RunLoop cancel_run_loop;
  EXPECT_CALL(nearby_connections_, CancelPayload)
      .WillOnce([&](const std::string& service_id, int64_t payload_id,
                    NearbyConnectionsMojom::CancelPayloadCallback callback) {
        EXPECT_EQ(kServiceId, service_id);
        EXPECT_EQ(kPayloadId, payload_id);

        std::move(callback).Run(Status::kSuccess);
        cancel_run_loop.Quit();
      });

  base::RunLoop payload_run_loop;
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_, testing::_))
      .WillOnce([&](MockPayloadStatusListener::PayloadTransferUpdatePtr update,
                    std::optional<Medium> upgraded_medium) {
        EXPECT_EQ(kPayloadId, update->payload_id);
        EXPECT_EQ(PayloadStatus::kCanceled, update->status);
        EXPECT_EQ(0u, update->total_bytes);
        EXPECT_EQ(0u, update->bytes_transferred);
        EXPECT_EQ(std::nullopt, upgraded_medium);
        payload_run_loop.Quit();
      });

  nearby_connections_manager_->Cancel(kPayloadId);
  payload_run_loop.Run();
  cancel_run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest,
       ConnectCancelPayload_MultiplePayloads_HandleDestroyedPayloadListener) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  Connect(connection_listener_remote, payload_listener_remote,
          ConnectionResponse::kAccepted);

  // Send two payloads with the same listener. We will eventually cancel both
  // payloads, but we will reset the listener before cancelling the second
  // payload. This can happen in practice: if the first payload is cancelled or
  // fails, it makes sense to clean everything up before waiting for the other
  // payload cancellation/failure signals. We are testing that the connections
  // manager handles the missing listener gracefully.
  auto payload_listener =
      std::make_unique<testing::NiceMock<MockPayloadStatusListener>>();
  SendPayload(kPayloadId, *payload_listener);
  SendPayload(kPayloadId2, *payload_listener);

  base::RunLoop cancel_run_loop;
  EXPECT_CALL(nearby_connections_, CancelPayload)
      .Times(2)
      .WillOnce([&](const std::string& service_id, int64_t payload_id,
                    NearbyConnectionsMojom::CancelPayloadCallback callback) {
        EXPECT_EQ(kServiceId, service_id);
        EXPECT_EQ(kPayloadId, payload_id);

        std::move(callback).Run(Status::kSuccess);
      })
      .WillOnce([&](const std::string& service_id, int64_t payload_id,
                    NearbyConnectionsMojom::CancelPayloadCallback callback) {
        EXPECT_EQ(kServiceId, service_id);
        EXPECT_EQ(kPayloadId2, payload_id);

        std::move(callback).Run(Status::kSuccess);
        cancel_run_loop.Quit();
      });

  // Because the payload listener is reset before the second payload is
  // cancelled, we can only receive the first status update.
  base::RunLoop payload_run_loop;
  EXPECT_CALL(*payload_listener, OnStatusUpdate(testing::_, testing::_))
      .Times(1)
      .WillOnce([&](MockPayloadStatusListener::PayloadTransferUpdatePtr update,
                    std::optional<Medium> upgraded_medium) {
        EXPECT_EQ(kPayloadId, update->payload_id);
        EXPECT_EQ(PayloadStatus::kCanceled, update->status);
        EXPECT_EQ(0u, update->total_bytes);
        EXPECT_EQ(0u, update->bytes_transferred);
        EXPECT_EQ(std::nullopt, upgraded_medium);

        // Destroy the PayloadStatusListener after the first payload is
        // cancelled.
        payload_listener.reset();

        payload_run_loop.Quit();
      });

  nearby_connections_manager_->Cancel(kPayloadId);
  nearby_connections_manager_->Cancel(kPayloadId2);
  payload_run_loop.Run();
  cancel_run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectTimeout) {
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will time out.
  const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                 std::end(kEndpointInfo));

  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  NearbyConnectionsMojom::RequestConnectionCallback connect_callback;
  EXPECT_CALL(nearby_connections_, RequestConnection)
      .WillOnce(
          [&](const std::string& service_id,
              const std::vector<uint8_t>& endpoint_info,
              const std::string& endpoint_id, ConnectionOptionsPtr options,
              mojo::PendingRemote<ConnectionLifecycleListener> listener,
              NearbyConnectionsMojom::RequestConnectionCallback callback) {
            EXPECT_EQ(kServiceId, service_id);
            EXPECT_EQ(local_endpoint_info, endpoint_info);
            EXPECT_EQ(kRemoteEndpointId, endpoint_id);

            connection_listener_remote.Bind(std::move(listener));
            // Do not call callback until connection timed out.
            connect_callback = std::move(callback);
          });

  // Timing out should call disconnect.
  EXPECT_CALL(nearby_connections_, DisconnectFromEndpoint)
      .WillOnce(
          [&](const std::string& service_id, const std::string& endpoint_id,
              NearbyConnectionsMojom::DisconnectFromEndpointCallback callback) {
            EXPECT_EQ(kServiceId, service_id);
            EXPECT_EQ(kRemoteEndpointId, endpoint_id);
            std::move(callback).Run(Status::kSuccess);
          });

  base::RunLoop run_loop;
  NearbyConnection* nearby_connection = nullptr;
  nearby_connections_manager_->Connect(
      local_endpoint_info, kRemoteEndpointId,
      /*bluetooth_mac_address=*/std::nullopt,
      NearbyConnectionsManager::DataUsage::kOffline,
      base::BindLambdaForTesting([&](NearbyConnection* connection) {
        nearby_connection = connection;
        run_loop.Quit();
      }));
  // Simulate time passing until timeout is reached.
  task_environment_.FastForwardBy(kInitiateNearbyConnectionTimeout);
  run_loop.Run();

  // Expect callback to be called with null connection.
  EXPECT_FALSE(nearby_connection);

  // Resolving the connect callback after timeout should do nothing.
  std::move(connect_callback).Run(Status::kSuccess);
}

TEST_F(NearbyConnectionsManagerImplTest, StartAdvertising) {
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_TRUE(connection);
}

TEST_F(NearbyConnectionsManagerImplTest, IncomingPayloadStatusListener) {
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_TRUE(connection);

  testing::NiceMock<MockPayloadStatusListener> payload_listener;
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId, payload_listener.GetWeakPtr());

  auto expected_update = PayloadTransferUpdate::New(
      kPayloadId, PayloadStatus::kInProgress, kTotalSize, kBytesTransferred);
  base::RunLoop payload_run_loop;
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_, testing::_))
      .WillOnce([&](MockPayloadStatusListener::PayloadTransferUpdatePtr update,
                    std::optional<Medium> upgraded_medium) {
        EXPECT_EQ(expected_update, update);
        EXPECT_EQ(std::nullopt, upgraded_medium);
        payload_run_loop.Quit();
      });

  payload_listener_remote->OnPayloadTransferUpdate(kRemoteEndpointId,
                                                   expected_update.Clone());
  payload_run_loop.Run();

  // After success status.
  base::RunLoop payload_run_loop_2;
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_, testing::_))
      .WillOnce([&payload_run_loop_2]() { payload_run_loop_2.Quit(); });

  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
  payload_run_loop_2.Run();

  // PayloadStatusListener will be unregistered and won't receive further
  // updates.
  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_, testing::_))
      .Times(0);

  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
}

TEST_F(
    NearbyConnectionsManagerImplTest,
    IncomingPayloadStatusListener_MultiplePayloads_HandleDestroyedPayloadListener) {
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_TRUE(connection);

  // Register three payloads with the same listener. This happens when multiple
  // payloads are included in the same transfer. Use both file and byte payloads
  // to ensure control-frame logic is not invoked for either.
  auto payload_listener =
      std::make_unique<testing::NiceMock<MockPayloadStatusListener>>();
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId, payload_listener->GetWeakPtr());
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId2, payload_listener->GetWeakPtr());
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId3, payload_listener->GetWeakPtr());
  base::File file1, file2;
  InitializeTemporaryFile(file1);
  InitializeTemporaryFile(file2);
  payload_listener_remote->OnPayloadReceived(
      kRemoteEndpointId,
      Payload::New(kPayloadId, PayloadContent::NewFile(
                                   FilePayload::New(std::move(file1)))));
  payload_listener_remote->OnPayloadReceived(
      kRemoteEndpointId,
      Payload::New(kPayloadId2, PayloadContent::NewFile(
                                    FilePayload::New(std::move(file2)))));
  const std::vector<uint8_t> byte_payload(std::begin(kBytePayload),
                                          std::end(kBytePayload));
  payload_listener_remote->OnPayloadReceived(
      kRemoteEndpointId,
      Payload::New(kPayloadId3,
                   PayloadContent::NewBytes(BytesPayload::New(byte_payload))));

  // Fail the first payload and destroy the payload listener. Then, send updates
  // that the second and third payloads succeeded; this is unlikely in practice,
  // but we test to ensure that no control-frame logic is exercised. Expect that
  // a status update is only sent for the first payload failure because the
  // listener does not exist afterwards.
  base::RunLoop payload_run_loop;
  EXPECT_CALL(*payload_listener, OnStatusUpdate(testing::_, testing::_))
      .Times(1)
      .WillOnce([&](MockPayloadStatusListener::PayloadTransferUpdatePtr update,
                    std::optional<Medium> upgraded_medium) {
        EXPECT_EQ(kPayloadId, update->payload_id);
        EXPECT_EQ(PayloadStatus::kFailure, update->status);
        EXPECT_EQ(kTotalSize, update->total_bytes);
        EXPECT_EQ(0u, update->bytes_transferred);
        EXPECT_EQ(std::nullopt, upgraded_medium);

        // Destroy the PayloadStatusListener after the first payload fails.
        payload_listener.reset();
      });
  // Ensure that no control-frame logic is run, which can happen when a payload
  // update is received for an unregistered payload.
  EXPECT_CALL(nearby_connections_, CancelPayload).Times(0);

  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kFailure,
                                 kTotalSize, /*bytes_transferred=*/0u));
  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId2, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/0u));
  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId3, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/0u));
  payload_run_loop.RunUntilIdle();

  // Ensure that no control-frame logic was run, notably that no bytes were
  // written to the connection. Closing the connection will invoke any
  // outstanding Read() callback if there are no bytes to read.
  base::RunLoop read_run_loop;
  connection->Read(base::BindLambdaForTesting(
      [&](std::optional<std::vector<uint8_t>> bytes) {
        EXPECT_FALSE(bytes);
        read_run_loop.Quit();
      }));
  connection->Close();
  read_run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, IncomingRegisterPayloadPath) {
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_TRUE(connection);

  base::RunLoop register_payload_run_loop;
  EXPECT_CALL(nearby_connections_, RegisterPayloadFile)
      .WillOnce(
          [&](const std::string& service_id, int64_t payload_id,
              base::File input_file, base::File output_file,
              NearbyConnectionsMojom::RegisterPayloadFileCallback callback) {
            EXPECT_EQ(kServiceId, service_id);
            EXPECT_EQ(kPayloadId, payload_id);
            ASSERT_TRUE(input_file.IsValid());
            ASSERT_TRUE(output_file.IsValid());
            VerifyFileReadWrite(input_file, output_file);

            std::move(callback).Run(Status::kSuccess);
          });

  base::FilePath path;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::CreateTemporaryFile(&path));
  }

  NearbyConnectionsManager::ConnectionsCallback callback =
      base::BindLambdaForTesting([&register_payload_run_loop](Status status) {
        EXPECT_EQ(status, Status::kSuccess);
        register_payload_run_loop.Quit();
      });

  nearby_connections_manager_->RegisterPayloadPath(kPayloadId, path,
                                                   std::move(callback));

  register_payload_run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, IncomingBytesPayload) {
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_TRUE(connection);

  testing::NiceMock<MockPayloadStatusListener> payload_listener;
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId, payload_listener.GetWeakPtr());

  const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                              std::end(kPayload));

  payload_listener_remote->OnPayloadReceived(
      kRemoteEndpointId,
      Payload::New(kPayloadId, PayloadContent::NewBytes(
                                   BytesPayload::New(expected_payload))));

  base::RunLoop payload_run_loop;
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_, testing::_))
      .WillOnce([&payload_run_loop]() { payload_run_loop.Quit(); });

  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
  payload_run_loop.Run();

  Payload* payload =
      nearby_connections_manager_->GetIncomingPayload(kPayloadId);
  ASSERT_TRUE(payload);
  ASSERT_TRUE(payload->content->is_bytes());
  EXPECT_EQ(expected_payload, payload->content->get_bytes()->bytes);
}

TEST_F(NearbyConnectionsManagerImplTest, IncomingFilePayload) {
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_TRUE(connection);

  testing::NiceMock<MockPayloadStatusListener> payload_listener;
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId, payload_listener.GetWeakPtr());

  const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                              std::end(kPayload));

  base::File file;
  InitializeTemporaryFile(file);

  payload_listener_remote->OnPayloadReceived(
      kRemoteEndpointId,
      Payload::New(kPayloadId,
                   PayloadContent::NewFile(FilePayload::New(std::move(file)))));

  base::RunLoop payload_run_loop;
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_, testing::_))
      .WillOnce([&payload_run_loop]() { payload_run_loop.Quit(); });

  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
  payload_run_loop.Run();

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    Payload* payload =
        nearby_connections_manager_->GetIncomingPayload(kPayloadId);
    ASSERT_TRUE(payload);
    ASSERT_TRUE(payload->content->is_file());
    std::vector<uint8_t> payload_bytes(
        payload->content->get_file()->file.GetLength());
    EXPECT_TRUE(payload->content->get_file()->file.ReadAndCheck(
        /*offset=*/0, base::make_span(payload_bytes)));
    EXPECT_EQ(expected_payload, payload_bytes);
  }
}

TEST_F(NearbyConnectionsManagerImplTest, ClearIncomingPayloads) {
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_TRUE(connection);

  testing::NiceMock<MockPayloadStatusListener> payload_listener;
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId, payload_listener.GetWeakPtr());

  base::File file;
  InitializeTemporaryFile(file);
  payload_listener_remote->OnPayloadReceived(
      kRemoteEndpointId,
      Payload::New(kPayloadId,
                   PayloadContent::NewFile(FilePayload::New(std::move(file)))));

  base::RunLoop payload_run_loop;
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_, testing::_))
      .WillOnce([&payload_run_loop]() { payload_run_loop.Quit(); });

  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
  payload_run_loop.Run();

  nearby_connections_manager_->ClearIncomingPayloads();

  EXPECT_FALSE(nearby_connections_manager_->GetIncomingPayload(kPayloadId));
}

TEST_F(NearbyConnectionsManagerImplTest, ClearIncomingPayloadWithId) {
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_TRUE(connection);

  testing::NiceMock<MockPayloadStatusListener> payload_listener;
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId, payload_listener.GetWeakPtr());
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId2, payload_listener.GetWeakPtr());

  base::File file_1, file_2;
  InitializeTemporaryFile(file_1);
  InitializeTemporaryFile(file_2);
  payload_listener_remote->OnPayloadReceived(
      kRemoteEndpointId,
      Payload::New(kPayloadId, PayloadContent::NewFile(
                                   FilePayload::New(std::move(file_1)))));
  payload_listener_remote->OnPayloadReceived(
      kRemoteEndpointId,
      Payload::New(kPayloadId2, PayloadContent::NewFile(
                                    FilePayload::New(std::move(file_2)))));

  base::RunLoop payload_run_loop;
  testing::InSequence seq;
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_, testing::_));
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_, testing::_))
      .WillOnce([&payload_run_loop]() { payload_run_loop.Quit(); });

  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
  payload_listener_remote->OnPayloadTransferUpdate(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId2, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
  payload_run_loop.Run();

  nearby_connections_manager_->ClearIncomingPayloadWithId(kPayloadId);

  EXPECT_FALSE(nearby_connections_manager_->GetIncomingPayload(kPayloadId));
  EXPECT_TRUE(nearby_connections_manager_->GetIncomingPayload(kPayloadId2));
}

/******************************************************************************/
// Begin: NearbyConnectionsManagerImplTestMediums
/******************************************************************************/
using MediumsTestParam = std::tuple<NearbyConnectionsManager::PowerLevel,
                                    NearbyConnectionsManager::DataUsage,
                                    net::NetworkChangeNotifier::ConnectionType,
                                    bool,
                                    bool>;
class NearbyConnectionsManagerImplTestMediums
    : public NearbyConnectionsManagerImplTest,
      public testing::WithParamInterface<MediumsTestParam> {};

TEST_P(NearbyConnectionsManagerImplTestMediums, StartAdvertising_Options) {
  const MediumsTestParam& param = GetParam();
  NearbyConnectionsManager::PowerLevel power_level = std::get<0>(param);
  NearbyConnectionsManager::DataUsage data_usage = std::get<1>(param);
  net::NetworkChangeNotifier::ConnectionType connection_type =
      std::get<2>(param);
  bool is_webrtc_enabled = std::get<3>(GetParam());
  bool is_ble_v2_enabled = std::get<4>(GetParam());
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatureStates(
      {{features::kNearbySharingWebRtc, is_webrtc_enabled},
       {features::kEnableNearbyBleV2, is_ble_v2_enabled}});

  network_notifier_->SetConnectionType(connection_type);
  network_notifier_->SetUseDefaultConnectionCostImplementation(true);
  should_use_web_rtc_ =
      is_webrtc_enabled &&
      data_usage != NearbyConnectionsManager::DataUsage::kOffline &&
      connection_type != net::NetworkChangeNotifier::CONNECTION_NONE &&
      (data_usage != NearbyConnectionsManager::DataUsage::kWifiOnly ||
       (net::NetworkChangeNotifier::GetConnectionCost() !=
        net::NetworkChangeNotifier::CONNECTION_COST_METERED));

  bool is_high_power =
      power_level == NearbyConnectionsManager::PowerLevel::kHighPower;
  bool use_ble = is_ble_v2_enabled || !is_high_power;

  // TODO(crbug.com/1129069): Update when WiFi LAN is supported.
  auto expected_mediums = MediumSelection::New(
      /*bluetooth=*/is_high_power,
      /*ble=*/use_ble,
      /*web_rtc=*/should_use_web_rtc_,
      /*wifi_lan=*/false,
      /*wifi_direct=*/false);

  base::RunLoop run_loop;
  const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                 std::end(kEndpointInfo));
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;

  NearbyConnectionsManager::ConnectionsCallback callback =
      base::BindLambdaForTesting([&run_loop](Status status) {
        EXPECT_EQ(Status::kSuccess, status);
        run_loop.Quit();
      });

  EXPECT_CALL(nearby_connections_, StartAdvertising)
      .WillOnce([&](const std::string& service_id,
                    const std::vector<uint8_t>& endpoint_info,
                    AdvertisingOptionsPtr options,
                    mojo::PendingRemote<ConnectionLifecycleListener> listener,
                    NearbyConnectionsMojom::StartAdvertisingCallback callback) {
        EXPECT_EQ(is_high_power, options->auto_upgrade_bandwidth);
        EXPECT_EQ(expected_mediums, options->allowed_mediums);
        EXPECT_EQ(use_ble, options->enable_bluetooth_listening);
        EXPECT_EQ(is_high_power && should_use_web_rtc_,
                  options->enable_webrtc_listening);
        // BLE v2 expects this field to be null when in high power mode.
        EXPECT_EQ(!is_ble_v2_enabled || !is_high_power,
                  options->fast_advertisement_service_uuid.has_value());
        std::move(callback).Run(Status::kSuccess);
      });

  nearby_connections_manager_->StartAdvertising(
      local_endpoint_info, &incoming_connection_listener, power_level,
      data_usage, std::move(callback));

  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(
    NearbyConnectionsManagerImplTestMediums,
    NearbyConnectionsManagerImplTestMediums,
    testing::Combine(
        testing::Values(NearbyConnectionsManager::PowerLevel::kLowPower,
                        NearbyConnectionsManager::PowerLevel::kHighPower),
        testing::Values(NearbyConnectionsManager::DataUsage::kWifiOnly,
                        NearbyConnectionsManager::DataUsage::kOffline,
                        NearbyConnectionsManager::DataUsage::kOnline),
        testing::Values(net::NetworkChangeNotifier::CONNECTION_NONE,
                        net::NetworkChangeNotifier::CONNECTION_WIFI,
                        net::NetworkChangeNotifier::CONNECTION_3G),
        testing::Bool(),
        testing::Bool()));
/******************************************************************************/
// End: NearbyConnectionsManagerImplTestMediums
/******************************************************************************/

TEST_F(NearbyConnectionsManagerImplTest, StopAdvertising_BeforeStart) {
  EXPECT_CALL(nearby_connections_, StopAdvertising).Times(0);
  nearby_connections_manager_->StopAdvertising(base::BindOnce(
      [](Status status) { EXPECT_EQ(status, Status::kSuccess); }));
}

TEST_F(NearbyConnectionsManagerImplTest, StopAdvertising) {
  mojo::Remote<ConnectionLifecycleListener> listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(listener_remote, incoming_connection_listener);

  base::RunLoop run_loop;
  EXPECT_CALL(nearby_connections_, StopAdvertising)
      .WillOnce([&](const std::string& service_id,
                    NearbyConnectionsMojom::StopAdvertisingCallback callback) {
        EXPECT_EQ(kServiceId, service_id);
        std::move(callback).Run(Status::kSuccess);
        run_loop.Quit();
      });
  nearby_connections_manager_->StopAdvertising(base::BindOnce(
      [](Status status) { EXPECT_EQ(status, Status::kSuccess); }));
  run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, ShutdownAdvertising) {
  mojo::Remote<ConnectionLifecycleListener> listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(listener_remote, incoming_connection_listener);

  base::RunLoop run_loop;
  EXPECT_CALL(nearby_connections_, StopAllEndpoints)
      .WillOnce(
          [&](const std::string& service_id,
              NearbyConnectionsMojom::DisconnectFromEndpointCallback callback) {
            EXPECT_EQ(kServiceId, service_id);
            std::move(callback).Run(Status::kSuccess);
            run_loop.Quit();
          });
  nearby_connections_manager_->Shutdown();
  run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, ShutdownDiscoveryConnectionFails) {
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  base::RunLoop shutdown_run_loop;
  EXPECT_CALL(nearby_connections_, StopAllEndpoints)
      .WillOnce(
          [&](const std::string& service_id,
              NearbyConnectionsMojom::DisconnectFromEndpointCallback callback) {
            EXPECT_EQ(kServiceId, service_id);
            std::move(callback).Run(Status::kSuccess);
            shutdown_run_loop.Quit();
          });

  nearby_connections_manager_->Shutdown();
  shutdown_run_loop.Run();

  // RequestConnection will fail.
  const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                 std::end(kEndpointInfo));
  base::RunLoop connect_run_loop;
  NearbyConnection* nearby_connection;
  nearby_connections_manager_->Connect(
      local_endpoint_info, kRemoteEndpointId,
      /*bluetooth_mac_address=*/std::nullopt,
      NearbyConnectionsManager::DataUsage::kOffline,
      base::BindLambdaForTesting([&](NearbyConnection* connection) {
        nearby_connection = connection;
        connect_run_loop.Quit();
      }));
  connect_run_loop.Run();

  EXPECT_FALSE(nearby_connection);
}

TEST_F(NearbyConnectionsManagerImplTest,
       ShutdownBeforeStartDiscoveryOrAdvertising) {
  EXPECT_CALL(nearby_connections_, StopAllEndpoints).Times(0);
  nearby_connections_manager_->Shutdown();
}

TEST_F(NearbyConnectionsManagerImplTest,
       UpgradeBandwidthAfterAdvertisingSucceeds) {
  mojo::Remote<ConnectionLifecycleListener> listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(listener_remote, incoming_connection_listener);

  // Upgrading bandwidth will succeed.
  base::RunLoop run_loop;
  EXPECT_CALL(nearby_connections_, InitiateBandwidthUpgrade)
      .WillOnce([&](const std::string& service_id,
                    const std::string& endpoint_id,
                    NearbyConnectionsMojom::InitiateBandwidthUpgradeCallback
                        callback) {
        EXPECT_EQ(kServiceId, service_id);
        EXPECT_EQ(kRemoteEndpointId, endpoint_id);
        std::move(callback).Run(Status::kSuccess);
        run_loop.Quit();
      });
  nearby_connections_manager_->UpgradeBandwidth(kRemoteEndpointId);
  run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest,
       UpgradeBandwidthAfterDiscoverySucceeds) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  EXPECT_TRUE(nearby_connection);

  // Upgrading bandwidth will succeed.
  base::RunLoop run_loop;
  EXPECT_CALL(nearby_connections_, InitiateBandwidthUpgrade)
      .WillOnce([&](const std::string& service_id,
                    const std::string& endpoint_id,
                    NearbyConnectionsMojom::InitiateBandwidthUpgradeCallback
                        callback) {
        EXPECT_EQ(kServiceId, service_id);
        EXPECT_EQ(kRemoteEndpointId, endpoint_id);
        std::move(callback).Run(Status::kSuccess);
        run_loop.Quit();
      });
  nearby_connections_manager_->UpgradeBandwidth(kRemoteEndpointId);
  run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest,
       UpgradeBandwidthBeforeStartDiscoveryOrAdvertising) {
  EXPECT_CALL(nearby_connections_, InitiateBandwidthUpgrade).Times(0);
  nearby_connections_manager_->UpgradeBandwidth(kRemoteEndpointId);
}

TEST_F(NearbyConnectionsManagerImplTest,
       DoNotUpgradeBandwidthIfWebRtcDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kNearbySharingWebRtc);

  mojo::Remote<ConnectionLifecycleListener> listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(listener_remote, incoming_connection_listener);

  // Bandwidth upgrade should not be attempted if WebRTC is disabled.
  EXPECT_CALL(nearby_connections_, InitiateBandwidthUpgrade).Times(0);
  nearby_connections_manager_->UpgradeBandwidth(kRemoteEndpointId);
}

// Regression test for b/303675257.
TEST_F(NearbyConnectionsManagerImplTest,
       DisconnectListenerDeletesNearbyConnectionsManager) {
  // StartDiscovery will succeed.
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  mojo::Remote<ConnectionLifecycleListener> connection_listener_remote;
  mojo::Remote<PayloadListener> payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);
  EXPECT_NE(nullptr, nearby_connections_manager_);

  // Close should invoke disconnection callback.
  nearby_connection->SetDisconnectionListener(base::BindLambdaForTesting(
      [&]() { nearby_connections_manager_.reset(); }));

  nearby_connection->Close();
  EXPECT_EQ(nullptr, nearby_connections_manager_);
}

TEST_F(NearbyConnectionsManagerImplTest, RequestConnectionV3Initiated) {
  nearby::presence::PresenceDevice presence_device = CreatePresenceDevice();

  base::RunLoop request_connection_run_loop;
  EXPECT_CALL(nearby_connections_, RequestConnectionV3)
      .WillOnce(
          [&](const std::string& service_id,
              ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
              ConnectionOptionsPtr options,
              mojo::PendingRemote<ConnectionListenerV3> listener,
              NearbyConnectionsMojom::RequestConnectionV3Callback callback) {
            EXPECT_EQ(kServiceId, service_id);
            EXPECT_EQ(remote_device->endpoint_id, kRemoteEndpointId);

            std::move(callback).Run(Status::kSuccess);
            request_connection_run_loop.Quit();
          });

  nearby_connections_manager_->ConnectV3(
      presence_device, NearbyConnectionsManager::DataUsage::kOffline,
      base::DoNothing());
  request_connection_run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, OnConnectionTimedOutV3) {
  mojo::Remote<ConnectionListenerV3> connection_listener_v3_remote;
  nearby::presence::PresenceDevice presence_device = CreatePresenceDevice();

  EXPECT_CALL(nearby_connections_, RequestConnectionV3)
      .WillOnce(
          [&](const std::string& service_id,
              ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
              ConnectionOptionsPtr options,
              mojo::PendingRemote<ConnectionListenerV3> listener,
              NearbyConnectionsMojom::RequestConnectionV3Callback callback) {
            EXPECT_EQ(kServiceId, service_id);
            EXPECT_EQ(remote_device->endpoint_id, kRemoteEndpointId);

            connection_listener_v3_remote.Bind(std::move(listener));

            std::move(callback).Run(Status::kSuccess);
          });

  EXPECT_CALL(nearby_connections_, DisconnectFromDeviceV3)
      .WillOnce(
          [&](const std::string& service_id,
              ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
              NearbyConnectionsMojom::DisconnectFromDeviceV3Callback callback) {
            EXPECT_EQ(kServiceId, service_id);
            EXPECT_EQ(remote_device->endpoint_id, kRemoteEndpointId);

            std::move(callback).Run(Status::kSuccess);
          });

  base::RunLoop on_connection_timed_out_run_loop;
  NearbyConnection* nearby_connection = nullptr;
  nearby_connections_manager_->ConnectV3(
      presence_device, NearbyConnectionsManager::DataUsage::kOffline,
      base::BindLambdaForTesting([&](NearbyConnection* connection) {
        nearby_connection = connection;
        on_connection_timed_out_run_loop.Quit();
      }));

  task_environment_.FastForwardBy(kInitiateNearbyConnectionTimeout);
  on_connection_timed_out_run_loop.Run();

  EXPECT_FALSE(nearby_connection);
  histogram_tester()->ExpectTimeBucketCount(
      "Nearby.Connections.V3.ConnectionResult.Success.Latency",
      kConnectV3ToSuccessfulConnectionLatency, 0);
}

TEST_F(NearbyConnectionsManagerImplTest, RequestConnectionV3Accept) {
  mojo::Remote<ConnectionListenerV3> connection_listener_v3_remote;
  mojo::Remote<PayloadListenerV3> payload_listener_v3_remote;

  nearby::presence::PresenceDevice presence_device = CreatePresenceDevice();

  ConnectV3(presence_device, connection_listener_v3_remote,
            payload_listener_v3_remote,
            nearby::connections::mojom::InitialConnectionInfoV3::New(
                kAuthenticationToken, kRawAuthenticationToken,
                /*is_incoming_connection=*/false,
                nearby::connections::mojom::AuthenticationStatus::kSuccess),
            /*on_connection_result_status=*/Status::kSuccess);

  histogram_tester()->ExpectBucketCount(
      "Nearby.Connections.V3.Connection.Result", Status::kSuccess, 1);
  histogram_tester()->ExpectTotalCount(
      "Nearby.Connections.V3.Connection.Result", 1);
  histogram_tester()->ExpectTimeBucketCount(
      "Nearby.Connections.V3.ConnectionResult.Success.Latency",
      kConnectV3ToSuccessfulConnectionLatency, 1);
}

TEST_F(NearbyConnectionsManagerImplTest, RequestConnectionV3Reject) {
  mojo::Remote<ConnectionListenerV3> connection_listener_v3_remote;
  mojo::Remote<PayloadListenerV3> payload_listener_v3_remote;

  nearby::presence::PresenceDevice presence_device = CreatePresenceDevice();

  ConnectV3(presence_device, connection_listener_v3_remote,
            payload_listener_v3_remote,
            nearby::connections::mojom::InitialConnectionInfoV3::New(
                kAuthenticationToken, kRawAuthenticationToken,
                /*is_incoming_connection=*/false,
                nearby::connections::mojom::AuthenticationStatus::kFailure),
            /*on_connection_result_status=*/Status::kSuccess);
}

TEST_F(NearbyConnectionsManagerImplTest, OnConnectionResultV3Rejected) {
  mojo::Remote<ConnectionListenerV3> connection_listener_v3_remote;
  mojo::Remote<PayloadListenerV3> payload_listener_v3_remote;

  nearby::presence::PresenceDevice presence_device = CreatePresenceDevice();

  ConnectV3(presence_device, connection_listener_v3_remote,
            payload_listener_v3_remote,
            nearby::connections::mojom::InitialConnectionInfoV3::New(
                kAuthenticationToken, kRawAuthenticationToken,
                /*is_incoming_connection=*/false,
                nearby::connections::mojom::AuthenticationStatus::kFailure),
            /*on_connection_result_status=*/Status::kError);

  histogram_tester()->ExpectBucketCount(
      "Nearby.Connections.V3.Connection.Result", Status::kError, 1);
  histogram_tester()->ExpectTotalCount(
      "Nearby.Connections.V3.Connection.Result", 1);
  histogram_tester()->ExpectTimeBucketCount(
      "Nearby.Connections.V3.ConnectionResult.Success.Latency",
      kConnectV3ToSuccessfulConnectionLatency, 0);
}

TEST_F(NearbyConnectionsManagerImplTest, DisconnectV3) {
  mojo::Remote<ConnectionListenerV3> connection_listener_v3_remote;
  mojo::Remote<PayloadListenerV3> payload_listener_v3_remote;

  nearby::presence::PresenceDevice presence_device = CreatePresenceDevice();

  ConnectV3(presence_device, connection_listener_v3_remote,
            payload_listener_v3_remote,
            nearby::connections::mojom::InitialConnectionInfoV3::New(
                kAuthenticationToken, kRawAuthenticationToken,
                /*is_incoming_connection=*/false,
                nearby::connections::mojom::AuthenticationStatus::kSuccess),
            /*on_connection_result_status=*/Status::kSuccess);

  histogram_tester()->ExpectTimeBucketCount(
      "Nearby.Connections.V3.ConnectionResult.Success.Latency",
      kConnectV3ToSuccessfulConnectionLatency, 1);

  base::RunLoop disconnect_run_loop;
  EXPECT_CALL(nearby_connections_, DisconnectFromDeviceV3)
      .WillOnce(
          [&](const std::string& service_id,
              ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
              NearbyConnectionsMojom::DisconnectFromDeviceV3Callback callback) {
            EXPECT_EQ(kServiceId, service_id);
            EXPECT_EQ(remote_device->endpoint_id, kRemoteEndpointId);

            std::move(callback).Run(Status::kSuccess);
            disconnect_run_loop.Quit();
          });

  nearby_connections_manager_->DisconnectV3(presence_device);
  disconnect_run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, OnConnectionRequestedV3) {
  nearby::presence::PresenceDevice presence_device = CreatePresenceDevice();

  base::RunLoop request_connection_run_loop;
  EXPECT_CALL(nearby_connections_, RequestConnectionV3)
      .WillOnce(
          [&](const std::string& service_id,
              ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
              ConnectionOptionsPtr options,
              mojo::PendingRemote<ConnectionListenerV3> listener,
              NearbyConnectionsMojom::RequestConnectionV3Callback callback) {
            EXPECT_EQ(kServiceId, service_id);
            EXPECT_EQ(remote_device->endpoint_id, kRemoteEndpointId);

            std::move(callback).Run(Status::kError);
            request_connection_run_loop.Quit();
          });

  nearby_connections_manager_->ConnectV3(
      presence_device, NearbyConnectionsManager::DataUsage::kOffline,
      base::DoNothing());
  request_connection_run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, OnBandwidthChangedV3) {
  mojo::Remote<ConnectionListenerV3> connection_listener_v3_remote;
  mojo::Remote<PayloadListenerV3> payload_listener_v3_remote;

  nearby::presence::PresenceDevice presence_device = CreatePresenceDevice();

  base::RunLoop request_connection_run_loop;
  EXPECT_CALL(nearby_connections_, RequestConnectionV3)
      .WillOnce(
          [&](const std::string& service_id,
              ash::nearby::presence::mojom::PresenceDevicePtr remote_device,
              ConnectionOptionsPtr options,
              mojo::PendingRemote<ConnectionListenerV3> listener,
              NearbyConnectionsMojom::RequestConnectionV3Callback callback) {
            EXPECT_EQ(kServiceId, service_id);
            EXPECT_EQ(remote_device->endpoint_id, kRemoteEndpointId);

            connection_listener_v3_remote.Bind(std::move(listener));
            std::move(callback).Run(Status::kSuccess);
            request_connection_run_loop.Quit();
          });

  ash::nearby::presence::mojom::PresenceDevicePtr presence_device_mojom =
      ash::nearby::presence::BuildPresenceMojomDevice(presence_device);
  nearby_connections_manager_->ConnectV3(
      presence_device, NearbyConnectionsManager::DataUsage::kOffline,
      base::DoNothing());
  request_connection_run_loop.Run();

  testing::NiceMock<MockBandwidthUpgradeListener> bandwidth_listener;
  nearby_connections_manager_->RegisterBandwidthUpgradeListener(
      bandwidth_listener.GetWeakPtr());

  base::RunLoop bandwidth_run_loop;
  EXPECT_CALL(bandwidth_listener,
              OnBandwidthUpgradeV3(
                  testing::An<nearby::presence::PresenceDevice>(), testing::_))
      .WillOnce([&](nearby::presence::PresenceDevice remote_device,
                    const Medium medium) {
        EXPECT_EQ(remote_device.GetEndpointId(), kRemoteEndpointId);
        EXPECT_EQ(medium, Medium::kWebRtc);

        bandwidth_run_loop.Quit();
      });

  // `OnBandwidthChangedV3()` needs to be called twice. The first one is always
  // called for the first Medium connected to and does not propagate to the
  // listener. The second call is when the bandwidth truly changed and is
  // propagated through to the listener.
  connection_listener_v3_remote->OnBandwidthChangedV3(
      presence_device.GetEndpointId(),
      nearby::connections::mojom::BandwidthInfo::New(BandwidthQuality::kMedium,
                                                     Medium::kBluetooth));
  histogram_tester()->ExpectTotalCount(
      "Nearby.Connections.V3.Medium.ChangedToMedium", 0);
  connection_listener_v3_remote->OnBandwidthChangedV3(
      presence_device.GetEndpointId(),
      nearby::connections::mojom::BandwidthInfo::New(BandwidthQuality::kHigh,
                                                     Medium::kWebRtc));
  bandwidth_run_loop.Run();
  histogram_tester()->ExpectBucketCount(
      "Nearby.Connections.V3.Medium.ChangedToMedium", Medium::kWebRtc, 1);
}

TEST_F(NearbyConnectionsManagerImplTest, PayloadListenerV3RemoteCallbacks) {
  mojo::Remote<ConnectionListenerV3> connection_listener_v3_remote;
  mojo::Remote<PayloadListenerV3> payload_listener_v3_remote;

  const std::vector<uint8_t> byte_payload(std::begin(kBytePayload),
                                          std::end(kBytePayload));

  nearby::presence::PresenceDevice presence_device = CreatePresenceDevice();

  ConnectV3(presence_device, connection_listener_v3_remote,
            payload_listener_v3_remote,
            nearby::connections::mojom::InitialConnectionInfoV3::New(
                kAuthenticationToken, kRawAuthenticationToken,
                /*is_incoming_connection=*/false,
                nearby::connections::mojom::AuthenticationStatus::kSuccess),
            /*on_connection_result_status=*/Status::kSuccess);

  testing::NiceMock<MockPayloadStatusListener> payload_listener;
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId, payload_listener.GetWeakPtr());

  base::RunLoop payload_transfer_update_run_loop;
  EXPECT_CALL(payload_listener, OnStatusUpdate(testing::_, testing::_))
      .WillOnce([&](MockPayloadStatusListener::PayloadTransferUpdatePtr update,
                    std::optional<Medium> upgraded_medium) {
        EXPECT_EQ(update->payload_id, kPayloadId);
        EXPECT_EQ(update->status, PayloadStatus::kSuccess);
        EXPECT_EQ(update->total_bytes, kTotalSize);
        EXPECT_EQ(update->bytes_transferred, kTotalSize);

        payload_transfer_update_run_loop.Quit();
      });

  payload_listener_v3_remote->OnPayloadReceivedV3(
      kRemoteEndpointId,
      Payload::New(kPayloadId,
                   PayloadContent::NewBytes(BytesPayload::New(byte_payload))));
  payload_listener_v3_remote->OnPayloadTransferUpdateV3(
      kRemoteEndpointId,
      PayloadTransferUpdate::New(kPayloadId, PayloadStatus::kSuccess,
                                 kTotalSize, /*bytes_transferred=*/kTotalSize));
  payload_transfer_update_run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest, InjectBluetoothEndpoint_Success) {
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  const std::vector<uint8_t> endpoint_info(std::begin(kEndpointInfo),
                                           std::end(kEndpointInfo));
  base::RunLoop run_loop;
  base::OnceCallback<void(nearby::connections::mojom::Status)> callback =
      base::BindLambdaForTesting(
          [&run_loop](nearby::connections::mojom::Status status) {
            EXPECT_EQ(status, nearby::connections::mojom::Status::kSuccess);
            run_loop.Quit();
          });
  EXPECT_CALL(
      nearby_connections_,
      InjectBluetoothEndpoint(testing::Eq(kServiceId), testing::Eq(kEndpointId),
                              testing::Eq(endpoint_info),
                              testing::Eq(kBluetoothMacAddress), testing::_))
      .WillOnce([&](const std::string& service_id,
                    const std::string& endpoint_id,
                    const std::vector<uint8_t> endpoint_info,
                    const std::vector<uint8_t> bluetooth_mac_address,
                    base::OnceCallback<void(nearby::connections::mojom::Status)>
                        callback) {
        EXPECT_EQ(kServiceId, service_id);
        EXPECT_EQ(kEndpointId, endpoint_id);
        EXPECT_EQ(std::vector<uint8_t>(std::begin(kEndpointInfo),
                                       std::end(kEndpointInfo)),
                  endpoint_info);
        EXPECT_EQ(kBluetoothMacAddress, bluetooth_mac_address);
        std::move(callback).Run(nearby::connections::mojom::Status::kSuccess);
      });

  nearby_connections_manager_->InjectBluetoothEndpoint(
      kServiceId, kEndpointId, endpoint_info, kBluetoothMacAddress,
      std::move(callback));
  run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest,
       InjectBluetoothEndpoint_Error_EndpointIdShort) {
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  const std::vector<uint8_t> endpoint_info(std::begin(kEndpointInfo),
                                           std::end(kEndpointInfo));
  base::RunLoop run_loop;
  base::OnceCallback<void(nearby::connections::mojom::Status)> callback =
      base::BindLambdaForTesting(
          [&run_loop](nearby::connections::mojom::Status status) {
            EXPECT_EQ(status, nearby::connections::mojom::Status::kError);
            run_loop.Quit();
          });

  // Provide 2-byte endpoint id instead of required 4 byte endpoint id.
  const std::string short_endpoint_id = "BS";

  nearby_connections_manager_->InjectBluetoothEndpoint(
      kServiceId, short_endpoint_id, endpoint_info, kBluetoothMacAddress,
      std::move(callback));
  run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest,
       InjectBluetoothEndpoint_Error_EndpointInfoSize0) {
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // Provide endpoint info size 0 to expect an error.
  const std::vector<uint8_t> endpoint_info;
  base::RunLoop run_loop;
  base::OnceCallback<void(nearby::connections::mojom::Status)> callback =
      base::BindLambdaForTesting(
          [&run_loop](nearby::connections::mojom::Status status) {
            EXPECT_EQ(status, nearby::connections::mojom::Status::kError);
            run_loop.Quit();
          });

  nearby_connections_manager_->InjectBluetoothEndpoint(
      kServiceId, kEndpointId, endpoint_info, kBluetoothMacAddress,
      std::move(callback));
  run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest,
       InjectBluetoothEndpoint_Error_EndpointInfoSize131) {
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // Provide endpoint info size >130 to expect an error.
  const std::vector<uint8_t> endpoint_info(131, 0);
  base::RunLoop run_loop;
  base::OnceCallback<void(nearby::connections::mojom::Status)> callback =
      base::BindLambdaForTesting(
          [&run_loop](nearby::connections::mojom::Status status) {
            EXPECT_EQ(status, nearby::connections::mojom::Status::kError);
            run_loop.Quit();
          });

  nearby_connections_manager_->InjectBluetoothEndpoint(
      kServiceId, kEndpointId, endpoint_info, kBluetoothMacAddress,
      std::move(callback));
  run_loop.Run();
}

TEST_F(NearbyConnectionsManagerImplTest,
       InjectBluetoothEndpoint_Error_BluetoothMacAddressSizeNot6) {
  mojo::Remote<EndpointDiscoveryListener> discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  const std::vector<uint8_t> endpoint_info(std::begin(kEndpointInfo),
                                           std::end(kEndpointInfo));
  base::RunLoop run_loop;
  base::OnceCallback<void(nearby::connections::mojom::Status)> callback =
      base::BindLambdaForTesting(
          [&run_loop](nearby::connections::mojom::Status status) {
            EXPECT_EQ(status, nearby::connections::mojom::Status::kError);
            run_loop.Quit();
          });

  // Provide a bluetooth mac address with size !=6 to expect an error.
  const std::vector<uint8_t> err_bluetooth_mac_address(7, 0);

  nearby_connections_manager_->InjectBluetoothEndpoint(
      kServiceId, kEndpointId, endpoint_info, err_bluetooth_mac_address,
      std::move(callback));
  run_loop.Run();
}
