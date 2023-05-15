// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_DEVICE_SERVICE_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_DEVICE_SERVICE_H_

#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace global_media_controls::test {

class MockDeviceService : public mojom::DeviceService {
 public:
  MockDeviceService();
  ~MockDeviceService() override;

  // Returns a remote bound to `this`.
  mojo::PendingRemote<mojom::DeviceService> PassRemote();

  // Resets the Mojo receiver bound to `this`.
  void ResetReceiver();

  // Flushes the Mojo receiver bound to `this`.
  void FlushForTesting();

  MOCK_METHOD(void,
              GetDeviceListHostForSession,
              (const std::string& session_id,
               mojo::PendingReceiver<mojom::DeviceListHost> host_receiver,
               mojo::PendingRemote<mojom::DeviceListClient> client_remote));
  MOCK_METHOD(void,
              GetDeviceListHostForPresentation,
              (mojo::PendingReceiver<mojom::DeviceListHost> host_receiver,
               mojo::PendingRemote<mojom::DeviceListClient> client_remote));
  MOCK_METHOD(
      void,
      SetDevicePickerProvider,
      (mojo::PendingRemote<mojom::DevicePickerProvider> provider_remote));

 private:
  mojo::Receiver<mojom::DeviceService> receiver_{this};
};

}  // namespace global_media_controls::test

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MOCK_DEVICE_SERVICE_H_
