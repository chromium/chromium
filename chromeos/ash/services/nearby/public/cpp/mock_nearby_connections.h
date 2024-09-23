// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_MOCK_NEARBY_CONNECTIONS_H_
#define CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_MOCK_NEARBY_CONNECTIONS_H_

#include "chromeos/ash/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/sharing.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gmock/include/gmock/gmock.h"

using NearbyConnectionsMojom = ::nearby::connections::mojom::NearbyConnections;
using AdvertisingOptionsPtr = ::nearby::connections::mojom::AdvertisingOptionsPtr;
using ConnectionLifecycleListener =
    ::nearby::connections::mojom::ConnectionLifecycleListener;
using ConnectionListenerV3 = ::nearby::connections::mojom::ConnectionListenerV3;
using ConnectionOptionsPtr = ::nearby::connections::mojom::ConnectionOptionsPtr;
using DiscoveryOptionsPtr = ::nearby::connections::mojom::DiscoveryOptionsPtr;
using EndpointDiscoveryListener =
    ::nearby::connections::mojom::EndpointDiscoveryListener;
using PayloadListener = ::nearby::connections::mojom::PayloadListener;
using PayloadListenerV3 = ::nearby::connections::mojom::PayloadListenerV3;
using PayloadPtr = ::nearby::connections::mojom::PayloadPtr;

namespace ash::nearby {

class MockNearbyConnections : public NearbyConnectionsMojom {
 public:
  MockNearbyConnections();
  MockNearbyConnections(const MockNearbyConnections&) = delete;
  MockNearbyConnections& operator=(const MockNearbyConnections&) = delete;
  ~MockNearbyConnections() override;

  const mojo::SharedRemote<NearbyConnectionsMojom>& shared_remote() const {
    return shared_remote_;
  }

  void BindInterface(
      mojo::PendingReceiver<NearbyConnectionsMojom> pending_receiver);

  MOCK_METHOD(void,
              StartAdvertising,
              (const std::string& service_id,
               const std::vector<uint8_t>& endpoint_info,
               AdvertisingOptionsPtr,
               mojo::PendingRemote<ConnectionLifecycleListener>,
               StartDiscoveryCallback),
              (override));
  MOCK_METHOD(void,
              StopAdvertising,
              (const std::string& service_id, StopAdvertisingCallback),
              (override));
  MOCK_METHOD(void,
              StartDiscovery,
              (const std::string& service_id,
               DiscoveryOptionsPtr,
               mojo::PendingRemote<EndpointDiscoveryListener>,
               StartDiscoveryCallback),
              (override));
  MOCK_METHOD(void,
              StopDiscovery,
              (const std::string& service_id, StopDiscoveryCallback),
              (override));
  MOCK_METHOD(void,
              InjectBluetoothEndpoint,
              (const std::string& service_id,
               const std::string& endpoint_id,
               const std::vector<uint8_t>& endpoint_info,
               const std::vector<uint8_t>& remote_bluetooth_mac_address,
               InjectBluetoothEndpointCallback callback),
              (override));
  MOCK_METHOD(void,
              RequestConnection,
              (const std::string& service_id,
               const std::vector<uint8_t>& endpoint_info,
               const std::string& endpoint_id,
               ConnectionOptionsPtr options,
               mojo::PendingRemote<ConnectionLifecycleListener>,
               RequestConnectionCallback),
              (override));
  MOCK_METHOD(void,
              DisconnectFromEndpoint,
              (const std::string& service_id,
               const std::string& endpoint_id,
               DisconnectFromEndpointCallback),
              (override));
  MOCK_METHOD(void,
              AcceptConnection,
              (const std::string& service_id,
               const std::string& endpoint_id,
               mojo::PendingRemote<PayloadListener> listener,
               AcceptConnectionCallback callback),
              (override));
  MOCK_METHOD(void,
              RejectConnection,
              (const std::string& service_id,
               const std::string& endpoint_id,
               RejectConnectionCallback callback),
              (override));
  MOCK_METHOD(void,
              SendPayload,
              (const std::string& service_id,
               const std::vector<std::string>& endpoint_ids,
               PayloadPtr payload,
               SendPayloadCallback callback),
              (override));
  MOCK_METHOD(void,
              CancelPayload,
              (const std::string& service_id,
               int64_t payload_id,
               CancelPayloadCallback callback),
              (override));
  MOCK_METHOD(void,
              StopAllEndpoints,
              (const std::string& service_id,
               DisconnectFromEndpointCallback callback),
              (override));
  MOCK_METHOD(void,
              InitiateBandwidthUpgrade,
              (const std::string& service_id,
               const std::string& endpoint_id,
               InitiateBandwidthUpgradeCallback callback),
              (override));
  MOCK_METHOD(void,
              RegisterPayloadFile,
              (const std::string& service_id,
               int64_t payload_id,
               base::File input_file,
               base::File output_file,
               RegisterPayloadFileCallback callback),
              (override));
  MOCK_METHOD(void,
              RequestConnectionV3,
              (const std::string& service_id,
               presence::mojom::PresenceDevicePtr remote_device,
               ConnectionOptionsPtr connection_options,
               mojo::PendingRemote<ConnectionListenerV3> listener,
               RequestConnectionV3Callback callback),
              (override));
  MOCK_METHOD(void,
              AcceptConnectionV3,
              (const std::string& service_id,
               presence::mojom::PresenceDevicePtr remote_device,
               mojo::PendingRemote<PayloadListenerV3> listener,
               AcceptConnectionV3Callback callback),
              (override));
  MOCK_METHOD(void,
              RejectConnectionV3,
              (const std::string& service_id,
               presence::mojom::PresenceDevicePtr remote_device,
               RejectConnectionV3Callback callback),
              (override));
  MOCK_METHOD(void,
              DisconnectFromDeviceV3,
              (const std::string& service_id,
               presence::mojom::PresenceDevicePtr remote_device,
               DisconnectFromDeviceV3Callback callback),
              (override));
  MOCK_METHOD(void,
              RegisterServiceWithPresenceDeviceProvider,
              (const std::string& service_id),
              (override));

 private:
  mojo::ReceiverSet<NearbyConnectionsMojom> receiver_set_;
  mojo::SharedRemote<NearbyConnectionsMojom> shared_remote_;
};

}  // namespace ash::nearby

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_MOCK_NEARBY_CONNECTIONS_H_
