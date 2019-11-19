// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_ISOLATED_DEVICE_PROVIDER_H_
#define CHROME_BROWSER_VR_SERVICE_ISOLATED_DEVICE_PROVIDER_H_

#include "base/containers/flat_map.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_provider.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace vr {

class VRUiHost;

class IsolatedVRDeviceProvider
    : public device::VRDeviceProvider,
      public device::mojom::IsolatedXRRuntimeProviderClient {
 public:
  IsolatedVRDeviceProvider();
  ~IsolatedVRDeviceProvider() override;

  // If the VR API requires initialization that should happen here.
  void Initialize(
      base::RepeatingCallback<void(
          device::mojom::XRDeviceId,
          device::mojom::VRDisplayInfoPtr,
          mojo::PendingRemote<device::mojom::XRRuntime>)> add_device_callback,
      base::RepeatingCallback<void(device::mojom::XRDeviceId)>
          remove_device_callback,
      base::OnceClosure initialization_complete) override;

  // Returns true if initialization is complete.
  bool Initialized() override;

 private:
  // IsolatedXRRuntimeProviderClient
  void OnDeviceAdded(
      mojo::PendingRemote<device::mojom::XRRuntime> device,
      mojo::PendingRemote<device::mojom::XRCompositorHost> compositor_host,
      device::mojom::XRDeviceId device_id) override;
  void OnDeviceRemoved(device::mojom::XRDeviceId id) override;
  void OnDevicesEnumerated() override;
  void OnServerError();
  void SetupDeviceProvider();

  bool initialized_ = false;
  int retry_count_ = 0;
  mojo::Remote<device::mojom::IsolatedXRRuntimeProvider> device_provider_;

  base::RepeatingCallback<void(device::mojom::XRDeviceId,
                               device::mojom::VRDisplayInfoPtr,
                               mojo::PendingRemote<device::mojom::XRRuntime>)>
      add_device_callback_;
  base::RepeatingCallback<void(device::mojom::XRDeviceId)>
      remove_device_callback_;
  base::OnceClosure initialization_complete_;
  mojo::Receiver<device::mojom::IsolatedXRRuntimeProviderClient> receiver_{
      this};

  using UiHostMap =
      base::flat_map<device::mojom::XRDeviceId, std::unique_ptr<VRUiHost>>;
  UiHostMap ui_host_map_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_ISOLATED_DEVICE_PROVIDER_H_
