// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_service_impl.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;

using blink::mojom::WebUsbService;
using device::FakeUsbDeviceInfo;
using device::mojom::UsbDeviceClient;
using device::mojom::UsbDeviceInfo;
using device::mojom::UsbDeviceInfoPtr;
using device::mojom::UsbDeviceManagerClient;

namespace {

const char kDefaultTestUrl[] = "https://www.google.com/";

ACTION_P2(ExpectGuidAndThen, expected_guid, callback) {
  ASSERT_TRUE(arg0);
  EXPECT_EQ(expected_guid, arg0->guid);
  if (!callback.is_null())
    callback.Run();
}

class WebUsbServiceImplTest : public ChromeRenderViewHostTestHarness {
 public:
  WebUsbServiceImplTest() {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  }

 protected:
  void SimulateDeviceServiceCrash() { device_manager()->CloseAllBindings(); }

  void ConnectToService(
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
    // Set fake device manager for UsbChooserContext.
    if (!device_manager()->IsBound()) {
      mojo::PendingRemote<device::mojom::UsbDeviceManager>
          pending_device_manager;
      device_manager()->AddReceiver(
          pending_device_manager.InitWithNewPipeAndPassReceiver());
      GetChooserContext()->SetDeviceManagerForTesting(
          std::move(pending_device_manager));
    }

    if (!web_usb_service_)
      web_usb_service_.reset(new WebUsbServiceImpl(main_rfh(), nullptr));

    web_usb_service_->BindReceiver(std::move(receiver));
  }

  UsbChooserContext* GetChooserContext() {
    return UsbChooserContextFactory::GetForProfile(profile());
  }

  device::FakeUsbDeviceManager* device_manager() {
    if (!device_manager_) {
      device_manager_ = std::make_unique<device::FakeUsbDeviceManager>();
    }
    return device_manager_.get();
  }

 private:
  std::unique_ptr<device::FakeUsbDeviceManager> device_manager_;
  std::unique_ptr<WebUsbServiceImpl> web_usb_service_;
  DISALLOW_COPY_AND_ASSIGN(WebUsbServiceImplTest);
};

class MockDeviceManagerClient : public UsbDeviceManagerClient {
 public:
  MockDeviceManagerClient() = default;
  ~MockDeviceManagerClient() override = default;

  mojo::PendingAssociatedRemote<UsbDeviceManagerClient>
  CreateInterfacePtrAndBind() {
    auto client = receiver_.BindNewEndpointAndPassRemote();
    receiver_.set_disconnect_handler(base::BindRepeating(
        &MockDeviceManagerClient::OnConnectionError, base::Unretained(this)));
    return client;
  }

  MOCK_METHOD1(DoOnDeviceAdded, void(UsbDeviceInfo*));
  void OnDeviceAdded(UsbDeviceInfoPtr device_info) override {
    DoOnDeviceAdded(device_info.get());
  }

  MOCK_METHOD1(DoOnDeviceRemoved, void(UsbDeviceInfo*));
  void OnDeviceRemoved(UsbDeviceInfoPtr device_info) override {
    DoOnDeviceRemoved(device_info.get());
  }

  MOCK_METHOD0(ConnectionError, void());
  void OnConnectionError() {
    receiver_.reset();
    ConnectionError();
  }

 private:
  mojo::AssociatedReceiver<UsbDeviceManagerClient> receiver_{this};
};

void ExpectDevicesAndThen(const std::set<std::string>& expected_guids,
                          base::OnceClosure continuation,
                          std::vector<UsbDeviceInfoPtr> results) {
  EXPECT_EQ(expected_guids.size(), results.size());
  std::set<std::string> actual_guids;
  for (size_t i = 0; i < results.size(); ++i)
    actual_guids.insert(results[i]->guid);
  EXPECT_EQ(expected_guids, actual_guids);
  std::move(continuation).Run();
}

}  // namespace

TEST_F(WebUsbServiceImplTest, NoPermissionDevice) {
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));

  auto device1 = base::MakeRefCounted<FakeUsbDeviceInfo>(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  auto device2 = base::MakeRefCounted<FakeUsbDeviceInfo>(
      0x1234, 0x5679, "ACME", "Frobinator+", "GHIJKL");
  auto no_permission_device1 = base::MakeRefCounted<FakeUsbDeviceInfo>(
      0xffff, 0x567b, "ACME", "Frobinator II", "MNOPQR");
  auto no_permission_device2 = base::MakeRefCounted<FakeUsbDeviceInfo>(
      0xffff, 0x567c, "ACME", "Frobinator Xtreme", "STUVWX");

  auto device_info_1 = device_manager()->AddDevice(device1);
  GetChooserContext()->GrantDevicePermission(origin, origin, *device_info_1);
  device_manager()->AddDevice(no_permission_device1);

  mojo::Remote<WebUsbService> web_usb_service;
  ConnectToService(web_usb_service.BindNewPipeAndPassReceiver());
  MockDeviceManagerClient mock_client;
  web_usb_service->SetClient(mock_client.CreateInterfacePtrAndBind());

  {
    // Call GetDevices once to make sure the WebUsbService is up and running
    // and the client is set or else we could block forever waiting for calls.
    // The site has no permission to access |no_permission_device1|, so result
    // of GetDevices() should only contain the |guid| of |device1|.
    std::set<std::string> guids;
    guids.insert(device1->guid());
    base::RunLoop loop;
    web_usb_service->GetDevices(
        base::BindOnce(&ExpectDevicesAndThen, guids, loop.QuitClosure()));
    loop.Run();
  }

  auto device_info_2 = device_manager()->AddDevice(device2);
  GetChooserContext()->GrantDevicePermission(origin, origin, *device_info_2);
  device_manager()->AddDevice(no_permission_device2);
  device_manager()->RemoveDevice(device1);
  device_manager()->RemoveDevice(device2);
  device_manager()->RemoveDevice(no_permission_device1);
  device_manager()->RemoveDevice(no_permission_device2);
  {
    base::RunLoop loop;
    base::RepeatingClosure barrier =
        base::BarrierClosure(2, loop.QuitClosure());
    testing::InSequence s;

    EXPECT_CALL(mock_client, DoOnDeviceRemoved(_))
        .WillOnce(ExpectGuidAndThen(device1->guid(), barrier))
        .WillOnce(ExpectGuidAndThen(device2->guid(), barrier));
    loop.Run();
  }

  device_manager()->AddDevice(device1);
  device_manager()->AddDevice(device2);
  device_manager()->AddDevice(no_permission_device1);
  device_manager()->AddDevice(no_permission_device2);
  {
    base::RunLoop loop;
    base::RepeatingClosure barrier =
        base::BarrierClosure(2, loop.QuitClosure());
    testing::InSequence s;

    EXPECT_CALL(mock_client, DoOnDeviceAdded(_))
        .WillOnce(ExpectGuidAndThen(device1->guid(), barrier))
        .WillOnce(ExpectGuidAndThen(device2->guid(), barrier));
    loop.Run();
  }
}

TEST_F(WebUsbServiceImplTest, ReconnectDeviceManager) {
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));

  auto* context = GetChooserContext();
  auto device = base::MakeRefCounted<FakeUsbDeviceInfo>(0x1234, 0x5678, "ACME",
                                                        "Frobinator", "ABCDEF");
  auto ephemeral_device = base::MakeRefCounted<FakeUsbDeviceInfo>(
      0, 0, "ACME", "Frobinator II", "");

  auto device_info = device_manager()->AddDevice(device);
  context->GrantDevicePermission(origin, origin, *device_info);
  auto ephemeral_device_info = device_manager()->AddDevice(ephemeral_device);
  context->GrantDevicePermission(origin, origin, *ephemeral_device_info);

  mojo::Remote<WebUsbService> web_usb_service;
  ConnectToService(web_usb_service.BindNewPipeAndPassReceiver());
  MockDeviceManagerClient mock_client;
  web_usb_service->SetClient(mock_client.CreateInterfacePtrAndBind());

  {
    std::set<std::string> guids;
    guids.insert(device->guid());
    guids.insert(ephemeral_device->guid());
    base::RunLoop loop;
    web_usb_service->GetDevices(
        base::BindOnce(&ExpectDevicesAndThen, guids, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_TRUE(context->HasDevicePermission(origin, origin, *device_info));
  EXPECT_TRUE(
      context->HasDevicePermission(origin, origin, *ephemeral_device_info));

  SimulateDeviceServiceCrash();
  EXPECT_CALL(mock_client, ConnectionError()).Times(1);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(context->HasDevicePermission(origin, origin, *device_info));
  EXPECT_FALSE(
      context->HasDevicePermission(origin, origin, *ephemeral_device_info));

  // Although a new device added, as the Device manager has been destroyed, no
  // event will be triggered.
  auto another_device = base::MakeRefCounted<FakeUsbDeviceInfo>(
      0x1234, 0x5679, "ACME", "Frobinator+", "GHIJKL");
  auto another_device_info = device_manager()->AddDevice(another_device);

  EXPECT_CALL(mock_client, DoOnDeviceAdded(_)).Times(0);
  base::RunLoop().RunUntilIdle();

  // Grant permission to the new device when service is off.
  context->GrantDevicePermission(origin, origin, *another_device_info);

  device_manager()->RemoveDevice(device);
  EXPECT_CALL(mock_client, DoOnDeviceRemoved(_)).Times(0);
  base::RunLoop().RunUntilIdle();

  // Reconnect the service.
  web_usb_service.reset();
  ConnectToService(web_usb_service.BindNewPipeAndPassReceiver());
  web_usb_service->SetClient(mock_client.CreateInterfacePtrAndBind());

  {
    std::set<std::string> guids;
    guids.insert(another_device->guid());
    base::RunLoop loop;
    web_usb_service->GetDevices(
        base::BindOnce(&ExpectDevicesAndThen, guids, loop.QuitClosure()));
    loop.Run();
  }

  EXPECT_TRUE(context->HasDevicePermission(origin, origin, *device_info));
  EXPECT_TRUE(
      context->HasDevicePermission(origin, origin, *another_device_info));
  EXPECT_FALSE(
      context->HasDevicePermission(origin, origin, *ephemeral_device_info));
}

TEST_F(WebUsbServiceImplTest, RevokeDevicePermission) {
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));

  auto* context = GetChooserContext();
  auto device_info = device_manager()->CreateAndAddDevice(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");

  mojo::Remote<WebUsbService> web_usb_service;
  ConnectToService(web_usb_service.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();
  {
    std::set<std::string> guids;
    base::RunLoop loop;
    web_usb_service->GetDevices(
        base::BindOnce(&ExpectDevicesAndThen, guids, loop.QuitClosure()));
    loop.Run();
  }

  context->GrantDevicePermission(origin, origin, *device_info);

  mojo::Remote<device::mojom::UsbDevice> device;
  web_usb_service->GetDevice(device_info->guid,
                             device.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(device);
  device.set_disconnect_handler(
      base::BindLambdaForTesting([&]() { device.reset(); }));

  auto objects = context->GetGrantedObjects(origin, origin);
  context->RevokeObjectPermission(origin, origin, objects[0]->value);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(device);
}
