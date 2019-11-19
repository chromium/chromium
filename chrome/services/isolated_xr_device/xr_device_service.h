// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_DEVICE_SERVICE_H_
#define CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_DEVICE_SERVICE_H_

#include "device/vr/public/mojom/browser_test_interfaces.mojom.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace device {

class XrDeviceService : public mojom::XRDeviceService {
 public:
  explicit XrDeviceService(
      mojo::PendingReceiver<mojom::XRDeviceService> receiver);
  ~XrDeviceService() override;

 private:
  // mojom::XRDeviceService implementation:
  void BindRuntimeProvider(
      mojo::PendingReceiver<mojom::IsolatedXRRuntimeProvider> receiver)
      override;
  void BindTestHook(mojo::PendingReceiver<device_test::mojom::XRServiceTestHook>
                        receiver) override;

  mojo::Receiver<mojom::XRDeviceService> receiver_;

  DISALLOW_COPY_AND_ASSIGN(XrDeviceService);
};

}  // namespace device

#endif  // CHROME_SERVICES_ISOLATED_XR_DEVICE_XR_DEVICE_SERVICE_H_
