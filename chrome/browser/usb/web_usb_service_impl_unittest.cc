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
#include "build/chromeos_buildflags.h"
#include "chrome/browser/usb/frame_usb_services.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS_ASH)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS_ASH)

using ::testing::_;

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
  WebUsbServiceImplTest() {}

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
  DISALLOW_COPY_AND_ASSIGN(WebUsbServiceImplTest);
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
  base::RunLoop run_loop;
  service->GetDevices(
      base::BindLambdaForTesting([&](std::vector<UsbDeviceInfoPtr> devices) {
        EXPECT_EQ(expected_guids.size(), devices.size());
        std::set<std::string> actual_guids;
        for (const auto& device : devices)
          actual_guids.insert(device->guid);
        EXPECT_EQ(expected_guids, actual_guids);
        run_loop.Quit();
      }));
  run_loop.Run();
}

void OpenDeviceBlocking(device::mojom::UsbDevice* device) {
  base::RunLoop run_loop;
  device->Open(
      base::BindLambdaForTesting([&](device::mojom::UsbOpenDeviceError error) {
        EXPECT_EQ(device::mojom::UsbOpenDeviceError::OK, error);
        run_loop.Quit();
      }));
  run_loop.Run();
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
  GetChooserContext()->GrantDevicePermission(origin, *device_info_1);
  device_manager()->AddDevice(no_permission_device1);

  mojo::Remote<WebUsbService> web_usb_service;
  ConnectToService(web_usb_service.BindNewPipeAndPassReceiver());
  MockDeviceManagerClient mock_client;
  web_usb_service->SetClient(mock_client.CreateInterfacePtrAndBind());

  // Call GetDevices once to make sure the WebUsbService is up and running
  // and the client is set or else we could block forever waiting for calls.
  // The site has no permission to access |no_permission_device1|, so result
  // of GetDevices() should only contain the |guid| of |device1|.
  GetDevicesBlocking(web_usb_service.get(), {device1->guid()});

  auto device_info_2 = device_manager()->AddDevice(device2);
  GetChooserContext()->GrantDevicePermission(origin, *device_info_2);
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
  context->GrantDevicePermission(origin, *device_info);
  auto ephemeral_device_info = device_manager()->AddDevice(ephemeral_device);
  context->GrantDevicePermission(origin, *ephemeral_device_info);

  mojo::Remote<WebUsbService> web_usb_service;
  ConnectToService(web_usb_service.BindNewPipeAndPassReceiver());
  MockDeviceManagerClient mock_client;
  web_usb_service->SetClient(mock_client.CreateInterfacePtrAndBind());

  GetDevicesBlocking(web_usb_service.get(),
                     {device->guid(), ephemeral_device->guid()});

  EXPECT_TRUE(context->HasDevicePermission(origin, *device_info));
  EXPECT_TRUE(context->HasDevicePermission(origin, *ephemeral_device_info));

  SimulateDeviceServiceCrash();
  EXPECT_CALL(mock_client, ConnectionError()).Times(1);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(context->HasDevicePermission(origin, *device_info));
  EXPECT_FALSE(context->HasDevicePermission(origin, *ephemeral_device_info));

  // Although a new device added, as the Device manager has been destroyed, no
  // event will be triggered.
  auto another_device = base::MakeRefCounted<FakeUsbDeviceInfo>(
      0x1234, 0x5679, "ACME", "Frobinator+", "GHIJKL");
  auto another_device_info = device_manager()->AddDevice(another_device);

  EXPECT_CALL(mock_client, DoOnDeviceAdded(_)).Times(0);
  base::RunLoop().RunUntilIdle();

  // Grant permission to the new device when service is off.
  context->GrantDevicePermission(origin, *another_device_info);

  device_manager()->RemoveDevice(device);
  EXPECT_CALL(mock_client, DoOnDeviceRemoved(_)).Times(0);
  base::RunLoop().RunUntilIdle();

  // Reconnect the service.
  web_usb_service.reset();
  ConnectToService(web_usb_service.BindNewPipeAndPassReceiver());
  web_usb_service->SetClient(mock_client.CreateInterfacePtrAndBind());

  GetDevicesBlocking(web_usb_service.get(), {another_device->guid()});

  EXPECT_TRUE(context->HasDevicePermission(origin, *device_info));
  EXPECT_TRUE(context->HasDevicePermission(origin, *another_device_info));
  EXPECT_FALSE(context->HasDevicePermission(origin, *ephemeral_device_info));
}

TEST_F(WebUsbServiceImplTest, RevokeDevicePermission) {
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));

  auto* context = GetChooserContext();
  auto device_info = device_manager()->CreateAndAddDevice(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");

  mojo::Remote<WebUsbService> web_usb_service;
  ConnectToService(web_usb_service.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();
  GetDevicesBlocking(web_usb_service.get(), {});

  context->GrantDevicePermission(origin, *device_info);

  mojo::Remote<device::mojom::UsbDevice> device;
  web_usb_service->GetDevice(device_info->guid,
                             device.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(device);
  device.set_disconnect_handler(
      base::BindLambdaForTesting([&]() { device.reset(); }));

  auto objects = context->GetGrantedObjects(origin);
  context->RevokeObjectPermission(origin, objects[0]->value);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(device);
}

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

  {
    base::RunLoop loop;
    device->SetConfiguration(1, base::BindLambdaForTesting([&](bool success) {
                               EXPECT_TRUE(success);
                               loop.Quit();
                             }));
    loop.Run();
  }

  {
    base::RunLoop loop;
    device->ClaimInterface(
        0, base::BindLambdaForTesting([&](UsbClaimInterfaceResult result) {
          EXPECT_EQ(result, UsbClaimInterfaceResult::kProtectedClass);
          loop.Quit();
        }));
    loop.Run();
  }

  {
    base::RunLoop loop;
    device->ClaimInterface(
        1, base::BindLambdaForTesting([&](UsbClaimInterfaceResult result) {
          EXPECT_EQ(result, UsbClaimInterfaceResult::kSuccess);
          loop.Quit();
        }));
    loop.Run();
  }
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

#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(WebUsbServiceImplTest, AllowlistedImprivataExtension) {
  extensions::DictionaryBuilder manifest;
  manifest.Set("name", "Fake Imprivata Extension")
      .Set("description", "For testing.")
      .Set("version", "0.1")
      .Set("manifest_version", 2)
      .Set("web_accessible_resources",
           extensions::ListBuilder().Append("index.html").Build());
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetManifest(manifest.Build())
          .SetID("dhodapiemamlmhlhblgcibabhdkohlen")
          .Build();
  ASSERT_TRUE(extension);
  extensions::ExtensionRegistry::Get(browser_context())->AddEnabled(extension);

  const GURL imprivata_url = extension->GetResourceURL("index.html");
  const auto imprivata_origin = url::Origin::Create(imprivata_url);

  auto* context = GetChooserContext();

  auto alternate_setting = device::mojom::UsbAlternateInterfaceInfo::New();
  alternate_setting->alternate_setting = 0;
  alternate_setting->class_code = 0x03;  // HID

  auto interface = device::mojom::UsbInterfaceInfo::New();
  interface->interface_number = 1;
  interface->alternates.push_back(std::move(alternate_setting));

  auto config = device::mojom::UsbConfigurationInfo::New();
  config->configuration_value = 1;
  config->interfaces.push_back(std::move(interface));

  std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
  configs.push_back(std::move(config));

  auto fake_device = base::MakeRefCounted<FakeUsbDeviceInfo>(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF", std::move(configs));

  auto device_info = device_manager()->AddDevice(fake_device);
  context->GrantDevicePermission(imprivata_origin, *device_info);

  NavigateAndCommit(imprivata_url);

  mojo::Remote<WebUsbService> service;
  ConnectToService(service.BindNewPipeAndPassReceiver());
  UsbTabHelper* tab_helper = UsbTabHelper::FromWebContents(web_contents());
  ASSERT_TRUE(tab_helper);

  GetDevicesBlocking(service.get(), {device_info->guid});

  mojo::Remote<device::mojom::UsbDevice> device;
  service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(tab_helper->IsDeviceConnected());

  OpenDeviceBlocking(device.get());

  {
    base::RunLoop loop;
    device->SetConfiguration(1, base::BindLambdaForTesting([&](bool success) {
                               EXPECT_TRUE(success);
                               loop.Quit();
                             }));
    loop.Run();
  }

  {
    base::RunLoop loop;
    device->ClaimInterface(
        1, base::BindLambdaForTesting([&](UsbClaimInterfaceResult result) {
          EXPECT_EQ(result, UsbClaimInterfaceResult::kSuccess);
          loop.Quit();
        }));
    loop.Run();
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS_ASH)
