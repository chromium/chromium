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

  mojo::Receiver<T>& receiver() { return receiver_; }

 private:
  mojo::Receiver<T> receiver_{this};
};

// This class should only be used by tests for GlobalMediaControls classes.
class MockDeviceListHost : public MockMojoImpl<mojom::DeviceListHost> {
 public:
  MockDeviceListHost();
  ~MockDeviceListHost() override;

  MOCK_METHOD(void, SelectDevice, (const std::string& device_id));
};

// This class should only be used by tests for GlobalMediaControls classes.
class MockDeviceListClient : public MockMojoImpl<mojom::DeviceListClient> {
 public:
  MockDeviceListClient();
  ~MockDeviceListClient() override;

  MOCK_METHOD(void, OnDevicesUpdated, (std::vector<mojom::DevicePtr> devices));
  MOCK_METHOD(void, OnPermissionRejected, ());
};

// This class should only be used by tests for GlobalMediaControls classes.
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

// This class should only be used by tests for GlobalMediaControls classes.
class MockDevicePickerProvider
    : public MockMojoImpl<mojom::DevicePickerProvider> {
 public:
  MockDevicePickerProvider();
  ~MockDevicePickerProvider() override;

  MOCK_METHOD(void, CreateItem, (const base::UnguessableToken& source_id));
  MOCK_METHOD(void, ShowItem, ());
  MOCK_METHOD(void, HideItem, ());
  MOCK_METHOD(void, DeleteItem, ());
  MOCK_METHOD(void,
              OnMetadataChanged,
              (const media_session::MediaMetadata& metadata));
  MOCK_METHOD(void,
              OnArtworkImageChanged,
              (const gfx::ImageSkia& artwork_image));
  MOCK_METHOD(void,
              OnFaviconImageChanged,
              (const gfx::ImageSkia& favicon_image));
  MOCK_METHOD(
      void,
      AddObserver,
      (mojo::PendingRemote<global_media_controls::mojom::DevicePickerObserver>
           observer));
  MOCK_METHOD(void, HideMediaUI, ());
};

// This class should only be used by tests for GlobalMediaControls classes.
class MockDevicePickerObserver
    : public MockMojoImpl<mojom::DevicePickerObserver> {
 public:
  MockDevicePickerObserver();
  ~MockDevicePickerObserver() override;

  MOCK_METHOD(void, OnMediaUIOpened, ());
  MOCK_METHOD(void, OnMediaUIClosed, ());
  MOCK_METHOD(void, OnMediaUIUpdated, ());
  MOCK_METHOD(void, OnPickerDismissed, ());
};

}  // namespace global_media_controls::test

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_DEVICE_SERVICE_H_
