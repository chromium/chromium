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

template <class T>
class MockMojoImpl : public T {
 public:
  MockMojoImpl() = default;
  ~MockMojoImpl() override = default;

  mojo::PendingRemote<T> PassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void BindReceiver(mojo::PendingReceiver<T> pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }

  void ResetReceiver() { receiver_.reset(); }

  void FlushForTesting() { receiver_.FlushForTesting(); }

 protected:
  mojo::Receiver<T> receiver_{this};
};

class MockDeviceListHost : public MockMojoImpl<mojom::DeviceListHost> {
 public:
  MockDeviceListHost();
  ~MockDeviceListHost() override;

  MOCK_METHOD(void, SelectDevice, (const std::string& device_id));
};

class MockDeviceService : public MockMojoImpl<mojom::DeviceService> {
 public:
  MockDeviceService();
  ~MockDeviceService() override;

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
};

}  // namespace global_media_controls::test

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MOCK_DEVICE_SERVICE_H_
