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
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/usb/frame_usb_services.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/back_forward_cache_util.h"
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
using ::testing::NiceMock;

using blink::mojom::WebUsbService;
using device::FakeUsbDeviceInfo;
using device::mojom::UsbClaimInterfaceResult;
using device::mojom::UsbDeviceClient;
using device::mojom::UsbDeviceInfo;
using device::mojom::UsbDeviceInfoPtr;
using device::mojom::UsbDeviceManagerClient;

namespace {

const char kDefaultTestUrl[] = "https://www.google.com/";
const char kCrossOriginTestUrl[] = "https://www.chromium.org";

ACTION_P2(ExpectGuidAndThen, expected_guid, callback) {
  ASSERT_TRUE(arg0);
  EXPECT_EQ(expected_guid, arg0->guid);
  if (!callback.is_null())
    callback.Run();
}

class WebUsbServiceImplTest : public ChromeRenderViewHostTestHarness {
 public:
  WebUsbServiceImplTest() = default;
  WebUsbServiceImplTest(const WebUsbServiceImplTest&) = delete;
  WebUsbServiceImplTest& operator=(const WebUsbServiceImplTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL(kDefaultTestUrl));
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

    FrameUsbServices::CreateFrameUsbServices(main_rfh(), std::move(receiver));
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
};

class MockDeviceManagerClient : public UsbDeviceManagerClient {
 public:
  MockDeviceManagerClient() = default;
  ~MockDeviceManagerClient() override = default;

  mojo::PendingAssociatedRemote<UsbDeviceManagerClient>
  CreateInterfacePtrAndBind() {
    auto client = receiver_.BindNewEndpointAndPassRemote();
    receiver_.set_disconnect_handler(base::BindOnce(
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

void GetDevicesBlocking(blink::mojom::WebUsbService* service,
                        const std::set<std::string>& expected_guids) {
  base::test::TestFuture<std::vector<UsbDeviceInfoPtr>> get_devices_future;
  service->GetDevices(get_devices_future.GetCallback());
  ASSERT_TRUE(get_devices_future.Wait());
  EXPECT_EQ(expected_guids.size(), get_devices_future.Get().size());
  std::set<std::string> actual_guids;
  for (const auto& device : get_devices_future.Get())
    actual_guids.insert(device->guid);
  EXPECT_EQ(expected_guids, actual_guids);
}

void OpenDeviceBlocking(device::mojom::UsbDevice* device) {
  base::test::TestFuture<device::mojom::UsbOpenDeviceError> open_future;
  device->Open(open_future.GetCallback());
  EXPECT_EQ(open_future.Get(), device::mojom::UsbOpenDeviceError::OK);
}

}  // namespace

TEST_F(WebUsbServiceImplTest, OpenAndCloseDevice) {
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));

  auto* context = GetChooserContext();
  auto device_info = device_manager()->CreateAndAddDevice(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  context->GrantDevicePermission(origin, *device_info);

  mojo::Remote<WebUsbService> service;
  ConnectToService(service.BindNewPipeAndPassReceiver());
  UsbTabHelper* tab_helper = UsbTabHelper::FromWebContents(web_contents());
  ASSERT_TRUE(tab_helper);

  GetDevicesBlocking(service.get(), {device_info->guid});

  mojo::Remote<device::mojom::UsbDevice> device;
  service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(tab_helper->IsDeviceConnected());

  OpenDeviceBlocking(device.get());
  EXPECT_TRUE(tab_helper->IsDeviceConnected());

  {
    base::RunLoop run_loop;
    device->Close(run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_FALSE(tab_helper->IsDeviceConnected());
}

TEST_F(WebUsbServiceImplTest, OpenAndDisconnectDevice) {
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));

  auto* context = GetChooserContext();
  auto fake_device = base::MakeRefCounted<FakeUsbDeviceInfo>(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  auto device_info = device_manager()->AddDevice(fake_device);
  context->GrantDevicePermission(origin, *device_info);

  mojo::Remote<WebUsbService> service;
  ConnectToService(service.BindNewPipeAndPassReceiver());
  UsbTabHelper* tab_helper = UsbTabHelper::FromWebContents(web_contents());
  ASSERT_TRUE(tab_helper);

  GetDevicesBlocking(service.get(), {device_info->guid});

  mojo::Remote<device::mojom::UsbDevice> device;
  service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(tab_helper->IsDeviceConnected());

  OpenDeviceBlocking(device.get());
  EXPECT_TRUE(tab_helper->IsDeviceConnected());

  device_manager()->RemoveDevice(fake_device);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_helper->IsDeviceConnected());
}

TEST_F(WebUsbServiceImplTest, OpenAndNavigateCrossOrigin) {
  // The test assumes the previous page gets deleted after navigation,
  // disconnecting the device. Disable back/forward cache to ensure that it
  // doesn't get preserved in the cache.
  // TODO(https://crbug.com/1220314): WebUSB actually already disables
  // back/forward cache in RenderFrameHostImpl::CreateWebUsbService(), but that
  // path is not triggered in unit tests, so this test fails. Fix this.
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));

  auto* context = GetChooserContext();
  auto fake_device = base::MakeRefCounted<FakeUsbDeviceInfo>(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  auto device_info = device_manager()->AddDevice(fake_device);
  context->GrantDevicePermission(origin, *device_info);

  mojo::Remote<WebUsbService> service;
  ConnectToService(service.BindNewPipeAndPassReceiver());
  UsbTabHelper* tab_helper = UsbTabHelper::FromWebContents(web_contents());
  ASSERT_TRUE(tab_helper);

  GetDevicesBlocking(service.get(), {device_info->guid});

  mojo::Remote<device::mojom::UsbDevice> device;
  service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(tab_helper->IsDeviceConnected());

  OpenDeviceBlocking(device.get());
  EXPECT_TRUE(tab_helper->IsDeviceConnected());

  NavigateAndCommit(GURL(kCrossOriginTestUrl));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tab_helper->IsDeviceConnected());
}

class WebUsbServiceImplProtectedInterfaceTest
    : public WebUsbServiceImplTest,
      public testing::WithParamInterface<uint8_t> {};

TEST_P(WebUsbServiceImplProtectedInterfaceTest, BlockProtectedInterface) {
  const auto kOrigin = url::Origin::Create(GURL(kDefaultTestUrl));

  auto* context = GetChooserContext();

  auto blocked_interface_alt = device::mojom::UsbAlternateInterfaceInfo::New();
  blocked_interface_alt->alternate_setting = 0;
  blocked_interface_alt->class_code = GetParam();

  auto blocked_interface = device::mojom::UsbInterfaceInfo::New();
  blocked_interface->interface_number = 0;
  blocked_interface->alternates.push_back(std::move(blocked_interface_alt));

  auto unblocked_interface_alt =
      device::mojom::UsbAlternateInterfaceInfo::New();
  unblocked_interface_alt->alternate_setting = 0;
  unblocked_interface_alt->class_code = 0xff;  // Vendor specific interface.

  auto unblocked_interface = device::mojom::UsbInterfaceInfo::New();
  unblocked_interface->interface_number = 1;
  unblocked_interface->alternates.push_back(std::move(unblocked_interface_alt));

  auto config = device::mojom::UsbConfigurationInfo::New();
  config->configuration_value = 1;
  config->interfaces.push_back(std::move(blocked_interface));
  config->interfaces.push_back(std::move(unblocked_interface));

  std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
  configs.push_back(std::move(config));

  auto fake_device = base::MakeRefCounted<FakeUsbDeviceInfo>(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF", std::move(configs));

  auto device_info = device_manager()->AddDevice(fake_device);
  context->GrantDevicePermission(kOrigin, *device_info);

  mojo::Remote<WebUsbService> service;
  ConnectToService(service.BindNewPipeAndPassReceiver());
  UsbTabHelper* tab_helper = UsbTabHelper::FromWebContents(web_contents());
  ASSERT_TRUE(tab_helper);

  GetDevicesBlocking(service.get(), {device_info->guid});

  mojo::Remote<device::mojom::UsbDevice> device;
  service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(tab_helper->IsDeviceConnected());

  OpenDeviceBlocking(device.get());

  base::test::TestFuture<bool> set_configuration_future;
  device->SetConfiguration(1, set_configuration_future.GetCallback());
  EXPECT_TRUE(set_configuration_future.Get());

  base::test::TestFuture<UsbClaimInterfaceResult> claim_interface0_future;
  device->ClaimInterface(0, claim_interface0_future.GetCallback());
  EXPECT_EQ(claim_interface0_future.Get(),
            UsbClaimInterfaceResult::kProtectedClass);

  base::test::TestFuture<UsbClaimInterfaceResult> claim_interface1_future;
  device->ClaimInterface(1, claim_interface1_future.GetCallback());
  EXPECT_EQ(claim_interface1_future.Get(), UsbClaimInterfaceResult::kSuccess);
}

INSTANTIATE_TEST_SUITE_P(
    WebUsbServiceImplProtectedInterfaceTests,
    WebUsbServiceImplProtectedInterfaceTest,
    testing::Values(0x01,    // Audio
                    0x03,    // HID
                    0x08,    // Mass Storage
                    0x0B,    // Smart Card
                    0x0E,    // Video
                    0x10,    // Audio/Video
                    0xE0));  // Wireless Controller (Bluetooth and Wireless USB)
