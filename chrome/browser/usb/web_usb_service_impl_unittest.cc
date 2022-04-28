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
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/usb/frame_usb_services.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/back_forward_cache_util.h"
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

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

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

scoped_refptr<FakeUsbDeviceInfo> CreateFakeHidDeviceInfo() {
  auto alternate_setting = device::mojom::UsbAlternateInterfaceInfo::New();
  alternate_setting->alternate_setting = 0;
  alternate_setting->class_code = 0x03;  // HID

  auto interface = device::mojom::UsbInterfaceInfo::New();
  interface->interface_number = 0;
  interface->alternates.push_back(std::move(alternate_setting));

  auto config = device::mojom::UsbConfigurationInfo::New();
  config->configuration_value = 1;
  config->interfaces.push_back(std::move(interface));

  std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
  configs.push_back(std::move(config));

  return base::MakeRefCounted<FakeUsbDeviceInfo>(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF", std::move(configs));
}

scoped_refptr<FakeUsbDeviceInfo> CreateFakeSmartCardDeviceInfo() {
  auto alternate_setting = device::mojom::UsbAlternateInterfaceInfo::New();
  alternate_setting->alternate_setting = 0;
  alternate_setting->class_code = 0x0B;  // Smart Card

  auto interface = device::mojom::UsbInterfaceInfo::New();
  interface->interface_number = 0;
  interface->alternates.push_back(std::move(alternate_setting));

  auto config = device::mojom::UsbConfigurationInfo::New();
  config->configuration_value = 1;
  config->interfaces.push_back(std::move(interface));

  std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
  configs.push_back(std::move(config));

  return base::MakeRefCounted<FakeUsbDeviceInfo>(
      0x4321, 0x8765, "ACME", "Frobinator", "ABCDEF", std::move(configs));
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
  NiceMock<MockDeviceManagerClient> mock_client;
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

#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS)
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

  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));
  extensions::ExtensionService* extension_service =
      extension_system->CreateExtensionService(
          base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
  extension_service->AddExtension(extension.get());

  const GURL imprivata_url = extension->GetResourceURL("index.html");
  const auto imprivata_origin = url::Origin::Create(imprivata_url);

  auto* context = GetChooserContext();

  auto device_info = device_manager()->AddDevice(CreateFakeHidDeviceInfo());
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

  base::test::TestFuture<bool> set_configuration_future;
  device->SetConfiguration(1, set_configuration_future.GetCallback());
  EXPECT_TRUE(set_configuration_future.Get());

  base::test::TestFuture<UsbClaimInterfaceResult> claim_interface_future;
  device->ClaimInterface(0, claim_interface_future.GetCallback());
  EXPECT_EQ(claim_interface_future.Get(), UsbClaimInterfaceResult::kSuccess);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(WebUsbServiceImplTest, AllowlistedSmartCardConnectorExtension) {
  extensions::DictionaryBuilder manifest;
  manifest.Set("name", "Fake Smart Card Connector Extension")
      .Set("description", "For testing.")
      .Set("version", "0.1")
      .Set("manifest_version", 2)
      .Set("web_accessible_resources",
           extensions::ListBuilder().Append("index.html").Build());
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetManifest(manifest.Build())
          .SetID("khpfeaanjngmcnplbdlpegiifgpfgdco")
          .Build();
  ASSERT_TRUE(extension);

  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));
  extensions::ExtensionService* extension_service =
      extension_system->CreateExtensionService(
          base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
  extension_service->AddExtension(extension.get());

  const GURL page_url = extension->GetResourceURL("index.html");
  const auto extension_origin = url::Origin::Create(page_url);

  // Add a smart card device. Also add an unrelated device, in order to test
  // that access is not automatically granted to it.
  auto ccid_device_info =
      device_manager()->AddDevice(CreateFakeSmartCardDeviceInfo());
  auto hid_device_info = device_manager()->AddDevice(CreateFakeHidDeviceInfo());

  // No need to grant permission. It is granted automatically for smart
  // card device.
  NavigateAndCommit(page_url);

  mojo::Remote<WebUsbService> service;
  ConnectToService(service.BindNewPipeAndPassReceiver());
  UsbTabHelper* tab_helper = UsbTabHelper::FromWebContents(web_contents());
  ASSERT_TRUE(tab_helper);

  // Check that the extensions is automatically granted access to the CCID
  // device and can claim its interfaces.
  {
    GetDevicesBlocking(service.get(), {ccid_device_info->guid});

    mojo::Remote<device::mojom::UsbDevice> device;
    service->GetDevice(ccid_device_info->guid,
                       device.BindNewPipeAndPassReceiver());
    OpenDeviceBlocking(device.get());

    base::test::TestFuture<bool> set_configuration_future;
    device->SetConfiguration(1, set_configuration_future.GetCallback());
    EXPECT_TRUE(set_configuration_future.Get());

    base::test::TestFuture<UsbClaimInterfaceResult> claim_interface_future;
    device->ClaimInterface(0, claim_interface_future.GetCallback());
    EXPECT_EQ(claim_interface_future.Get(), UsbClaimInterfaceResult::kSuccess);
  }

  // Check that the extension, if granted permission to a HID device can't claim
  // its interfaces.
  {
    GetChooserContext()->GrantDevicePermission(extension_origin,
                                               *hid_device_info);
    GetDevicesBlocking(service.get(),
                       {ccid_device_info->guid, hid_device_info->guid});

    mojo::Remote<device::mojom::UsbDevice> device;
    service->GetDevice(hid_device_info->guid,
                       device.BindNewPipeAndPassReceiver());
    OpenDeviceBlocking(device.get());

    base::test::TestFuture<bool> set_configuration_future;
    device->SetConfiguration(1, set_configuration_future.GetCallback());
    EXPECT_TRUE(set_configuration_future.Get());

    base::test::TestFuture<UsbClaimInterfaceResult> claim_interface_future;
    device->ClaimInterface(0, claim_interface_future.GetCallback());
    EXPECT_EQ(claim_interface_future.Get(),
              UsbClaimInterfaceResult::kProtectedClass);
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
