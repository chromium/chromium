// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLUETOOTH_CLASSIC_MEDIUM_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLUETOOTH_CLASSIC_MEDIUM_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "chrome/services/sharing/nearby/platform_v2/bluetooth_device.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/cpp/platform_v2/api/bluetooth_classic.h"

namespace location {
namespace nearby {
namespace chrome {

// Concrete BluetoothClassicMedium implementation.
// api::BluetoothClassicMedium is a synchronous interface, so this
// implementation consumes the synchronous signatures of
// bluetooth::mojom::Adapter methods.
class BluetoothClassicMedium : public api::BluetoothClassicMedium,
                               public bluetooth::mojom::AdapterObserver {
 public:
  explicit BluetoothClassicMedium(
      const mojo::SharedRemote<bluetooth::mojom::Adapter>& adapter);
  ~BluetoothClassicMedium() override;

  BluetoothClassicMedium(const BluetoothClassicMedium&) = delete;
  BluetoothClassicMedium& operator=(const BluetoothClassicMedium&) = delete;

  // api::BluetoothClassicMedium:
  bool StartDiscovery(DiscoveryCallback discovery_callback) override;
  bool StopDiscovery() override;
  std::unique_ptr<api::BluetoothSocket> ConnectToService(
      api::BluetoothDevice& remote_device,
      const std::string& service_uuid) override;
  std::unique_ptr<api::BluetoothServerSocket> ListenForService(
      const std::string& service_name,
      const std::string& service_uuid) override;
  BluetoothDevice* GetRemoteDevice(const std::string& mac_address) override;

 private:
  // bluetooth::mojom::AdapterObserver:
  void PresentChanged(bool present) override;
  void PoweredChanged(bool powered) override;
  void DiscoverableChanged(bool discoverable) override;
  void DiscoveringChanged(bool discovering) override;
  void DeviceAdded(bluetooth::mojom::DeviceInfoPtr device) override;
  void DeviceChanged(bluetooth::mojom::DeviceInfoPtr device) override;
  void DeviceRemoved(bluetooth::mojom::DeviceInfoPtr device) override;

  mojo::SharedRemote<bluetooth::mojom::Adapter> adapter_;

  // |adapter_observer_| is only set and bound during active discovery so that
  // events we don't care about outside of discovery don't pile up.
  mojo::Receiver<bluetooth::mojom::AdapterObserver> adapter_observer_{this};

  // These properties are only set while discovery is active.
  base::Optional<DiscoveryCallback> discovery_callback_;
  mojo::Remote<bluetooth::mojom::DiscoverySession> discovery_session_;

  std::map<std::string, chrome::BluetoothDevice>
      discovered_bluetooth_devices_map_;
};

}  // namespace chrome
}  // namespace nearby
}  // namespace location

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_V2_BLUETOOTH_CLASSIC_MEDIUM_H_
