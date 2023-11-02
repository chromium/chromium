// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "extensions/buildflags/buildflags.h"

#include "base/barrier_closure.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/command_line.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

using ::base::test::RunOnceCallback;
using ::base::test::RunOnceClosure;
using ::base::test::TestFuture;
using ::testing::_;
using ::testing::NiceMock;

constexpr base::StringPiece kDefaultTestUrl{"https://www.google.com/"};
constexpr base::StringPiece kCrossOriginTestUrl{"https://www.chromium.org"};

#if BUILDFLAG(ENABLE_EXTENSIONS)
constexpr char kAllowlistedImprivataExtensionId[] =
    "dhodapiemamlmhlhblgcibabhdkohlen";
constexpr char kAllowlistedSmartCardExtensionId[] =
    "khpfeaanjngmcnplbdlpegiifgpfgdco";
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

ACTION_P2(ExpectGuidAndThen, expected_guid, callback) {
  ASSERT_TRUE(arg0);
  EXPECT_EQ(expected_guid, arg0->guid);
  if (!callback.is_null())
    callback.Run();
}

// Calls GetDevices on `service` and blocks until the GetDevicesCallback is
// invoked. Fails the test if the guids of received UsbDeviceInfos do not
// exactly match `expected_guids`.
void GetDevicesBlocking(blink::mojom::WebUsbService* service,
                        const std::set<std::string>& expected_guids) {
  TestFuture<std::vector<device::mojom::UsbDeviceInfoPtr>> get_devices_future;
  service->GetDevices(get_devices_future.GetCallback());
  EXPECT_EQ(expected_guids.size(), get_devices_future.Get().size());
  std::set<std::string> actual_guids;
  for (const auto& device : get_devices_future.Get())
    actual_guids.insert(device->guid);
  EXPECT_EQ(expected_guids, actual_guids);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Creates a FakeUsbDeviceInfo with HID class code.
scoped_refptr<device::FakeUsbDeviceInfo> CreateFakeHidDeviceInfo() {
  auto alternate_setting = device::mojom::UsbAlternateInterfaceInfo::New();
  alternate_setting->alternate_setting = 0;
  alternate_setting->class_code = device::mojom::kUsbHidClass;

  auto interface = device::mojom::UsbInterfaceInfo::New();
  interface->interface_number = 0;
  interface->alternates.push_back(std::move(alternate_setting));

  auto config = device::mojom::UsbConfigurationInfo::New();
  config->configuration_value = 1;
  config->interfaces.push_back(std::move(interface));

  std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
  configs.push_back(std::move(config));

  return base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF", std::move(configs));
}

// Creates a FakeUsbDeviceInfo with Smart Card class code.
scoped_refptr<device::FakeUsbDeviceInfo> CreateFakeSmartCardDeviceInfo() {
  auto alternate_setting = device::mojom::UsbAlternateInterfaceInfo::New();
  alternate_setting->alternate_setting = 0;
  alternate_setting->class_code = device::mojom::kUsbSmartCardClass;

  auto interface = device::mojom::UsbInterfaceInfo::New();
  interface->interface_number = 0;
  interface->alternates.push_back(std::move(alternate_setting));

  auto config = device::mojom::UsbConfigurationInfo::New();
  config->configuration_value = 1;
  config->interfaces.push_back(std::move(interface));

  std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
  configs.push_back(std::move(config));

  return base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0x4321, 0x8765, "ACME", "Frobinator", "ABCDEF", std::move(configs));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// A mock UsbDeviceManagerClient implementation that can be used to listen for
// USB device connection events.
class MockDeviceManagerClient
    : public UsbChooserContext::UsbDeviceManagerClient {
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

  MOCK_METHOD1(OnDeviceAdded, void(device::mojom::UsbDeviceInfoPtr));
  MOCK_METHOD1(OnDeviceRemoved, void(device::mojom::UsbDeviceInfoPtr));

  MOCK_METHOD0(ConnectionError, void());
  void OnConnectionError() {
    receiver_.reset();
    ConnectionError();
  }

 private:
  mojo::AssociatedReceiver<UsbDeviceManagerClient> receiver_{this};
};

// Tests for embedder-specific behaviors of Chrome's blink::mojom::WebUsbService
// implementation.
class ChromeUsbDelegateTest : public ChromeRenderViewHostTestHarness {
 public:
  ChromeUsbDelegateTest() = default;
  ChromeUsbDelegateTest(const ChromeUsbDelegateTest&) = delete;
  ChromeUsbDelegateTest& operator=(const ChromeUsbDelegateTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL(kDefaultTestUrl));
  }

 protected:
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Creates a fake extension with the specified `extension_id` so that it can
  // exercise behaviors that are only enabled for privileged extensions.
  absl::optional<GURL> CreateExtensionWithId(base::StringPiece extension_id) {
    extensions::DictionaryBuilder manifest;
    manifest.Set("name", "Fake extension")
        .Set("description", "For testing.")
        .Set("version", "0.1")
        .Set("manifest_version", 2)
        .Set("web_accessible_resources",
             extensions::ListBuilder().Append("index.html").Build());
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder()
            .SetManifest(manifest.Build())
            .SetID(std::string(extension_id))
            .Build();
    if (!extension) {
      return absl::nullopt;
    }
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile()));
    extensions::ExtensionService* extension_service =
        extension_system->CreateExtensionService(
            base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service->AddExtension(extension.get());
    return extension->GetResourceURL("index.html");
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

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

    content::RenderFrameHostTester::For(main_rfh())
        ->CreateWebUsbServiceForTesting(std::move(receiver));
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

}  // namespace

TEST_F(ChromeUsbDelegateTest, NoPermissionDevice) {
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));

  auto device1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  auto device2 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0x1234, 0x5679, "ACME", "Frobinator+", "GHIJKL");
  auto no_permission_device1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0xffff, 0x567b, "ACME", "Frobinator II", "MNOPQR");
  auto no_permission_device2 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0xffff, 0x567c, "ACME", "Frobinator Xtreme", "STUVWX");

  // Connect two devices and grant permission for one.
  auto device_info_1 = device_manager()->AddDevice(device1);
  GetChooserContext()->GrantDevicePermission(origin, *device_info_1);
  device_manager()->AddDevice(no_permission_device1);

  // Create the WebUsbService and register a `mock_client` to receive
  // notifications on device connections and disconnections. GetDevices is
  // called to ensure the service is started and the client is set.
  mojo::Remote<blink::mojom::WebUsbService> web_usb_service;
  ConnectToService(web_usb_service.BindNewPipeAndPassReceiver());
  NiceMock<MockDeviceManagerClient> mock_client;
  web_usb_service->SetClient(mock_client.CreateInterfacePtrAndBind());
  GetDevicesBlocking(web_usb_service.get(), {device1->guid()});

  // Connect two more devices and grant permission for one.
  auto device_info_2 = device_manager()->AddDevice(device2);
  GetChooserContext()->GrantDevicePermission(origin, *device_info_2);
  device_manager()->AddDevice(no_permission_device2);

  // Disconnect all four devices. The `mock_client` should be notified only for
  // the devices it has permission to access.
  device_manager()->RemoveDevice(device1);
  device_manager()->RemoveDevice(device2);
  device_manager()->RemoveDevice(no_permission_device1);
  device_manager()->RemoveDevice(no_permission_device2);
  {
    base::RunLoop loop;
    base::RepeatingClosure barrier =
        base::BarrierClosure(2, loop.QuitClosure());
    testing::InSequence s;

    EXPECT_CALL(mock_client, OnDeviceRemoved)
        .WillOnce(ExpectGuidAndThen(device1->guid(), barrier))
        .WillOnce(ExpectGuidAndThen(device2->guid(), barrier));
    loop.Run();
  }

  // Reconnect all four devices. The `mock_client` should be notified only for
  // the devices it has permission to access.
  device_manager()->AddDevice(device1);
  device_manager()->AddDevice(device2);
  device_manager()->AddDevice(no_permission_device1);
  device_manager()->AddDevice(no_permission_device2);
  {
    base::RunLoop loop;
    base::RepeatingClosure barrier =
        base::BarrierClosure(2, loop.QuitClosure());
    testing::InSequence s;

    EXPECT_CALL(mock_client, OnDeviceAdded)
        .WillOnce(ExpectGuidAndThen(device1->guid(), barrier))
        .WillOnce(ExpectGuidAndThen(device2->guid(), barrier));
    loop.Run();
  }
}

TEST_F(ChromeUsbDelegateTest, ReconnectDeviceManager) {
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));

  auto* context = GetChooserContext();
  auto device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  auto ephemeral_device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0, 0, "ACME", "Frobinator II", "");

  // Connect two devices and grant permission for both. The first device is
  // eligible for persistent permissions and the second device is only eligible
  // for ephemeral permissions.
  auto device_info = device_manager()->AddDevice(device);
  context->GrantDevicePermission(origin, *device_info);
  auto ephemeral_device_info = device_manager()->AddDevice(ephemeral_device);
  context->GrantDevicePermission(origin, *ephemeral_device_info);

  // Create the WebUsbService and register a `mock_client` to receive
  // notifications on device connections and disconnections. GetDevices is
  // called to ensure the service is started and the client is set.
  mojo::Remote<blink::mojom::WebUsbService> web_usb_service;
  ConnectToService(web_usb_service.BindNewPipeAndPassReceiver());
  MockDeviceManagerClient mock_client;
  web_usb_service->SetClient(mock_client.CreateInterfacePtrAndBind());
  GetDevicesBlocking(web_usb_service.get(),
                     {device->guid(), ephemeral_device->guid()});
  EXPECT_TRUE(context->HasDevicePermission(origin, *device_info));
  EXPECT_TRUE(context->HasDevicePermission(origin, *ephemeral_device_info));

  // Simulate a device service crash. The ephemeral permission should be
  // revoked.
  SimulateDeviceServiceCrash();
  base::RunLoop loop;
  EXPECT_CALL(mock_client, ConnectionError()).WillOnce([&]() { loop.Quit(); });
  loop.Run();
  EXPECT_TRUE(context->HasDevicePermission(origin, *device_info));
  EXPECT_FALSE(context->HasDevicePermission(origin, *ephemeral_device_info));

  // Although a new device added, as the Device manager has been destroyed, no
  // event will be triggered.
  auto another_device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0x1234, 0x5679, "ACME", "Frobinator+", "GHIJKL");
  auto another_device_info = device_manager()->AddDevice(another_device);

  EXPECT_CALL(mock_client, OnDeviceAdded).Times(0);
  base::RunLoop().RunUntilIdle();

  // Grant permission to the new device when service is off.
  context->GrantDevicePermission(origin, *another_device_info);

  device_manager()->RemoveDevice(device);
  EXPECT_CALL(mock_client, OnDeviceRemoved).Times(0);
  base::RunLoop().RunUntilIdle();

  // Reconnect the service.
  web_usb_service.reset();
  base::RunLoop().RunUntilIdle();
  ConnectToService(web_usb_service.BindNewPipeAndPassReceiver());
  web_usb_service->SetClient(mock_client.CreateInterfacePtrAndBind());

  GetDevicesBlocking(web_usb_service.get(), {another_device->guid()});

  EXPECT_TRUE(context->HasDevicePermission(origin, *device_info));
  EXPECT_TRUE(context->HasDevicePermission(origin, *another_device_info));
  EXPECT_FALSE(context->HasDevicePermission(origin, *ephemeral_device_info));
}

TEST_F(ChromeUsbDelegateTest, RevokeDevicePermission) {
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));

  auto* context = GetChooserContext();

  // Connect a fake device.
  auto device_info = device_manager()->CreateAndAddDevice(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");

  // Create the WebUsbService and call GetDevices to ensure it is started.
  mojo::Remote<blink::mojom::WebUsbService> web_usb_service;
  ConnectToService(web_usb_service.BindNewPipeAndPassReceiver());
  GetDevicesBlocking(web_usb_service.get(), {});

  // Grant permission to access the connected device.
  context->GrantDevicePermission(origin, *device_info);
  auto objects = context->GetGrantedObjects(origin);
  ASSERT_EQ(1u, objects.size());

  // Connect the UsbDevice.
  mojo::Remote<device::mojom::UsbDevice> device;
  web_usb_service->GetDevice(device_info->guid,
                             device.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(device);

  // Revoke the permission. The UsbDevice should be disconnected.
  base::RunLoop disconnect_loop;
  device.set_disconnect_handler(disconnect_loop.QuitClosure());
  context->RevokeObjectPermission(origin, objects[0]->value);
  disconnect_loop.Run();
}

TEST_F(ChromeUsbDelegateTest, OpenAndCloseDevice) {
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));

  mojo::Remote<blink::mojom::WebUsbService> service;
  ConnectToService(service.BindNewPipeAndPassReceiver());

  // Connect a device and grant permission to access it.
  auto device_info = device_manager()->CreateAndAddDevice(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  GetChooserContext()->GrantDevicePermission(origin, *device_info);
  device::MockUsbMojoDevice mock_device;
  device_manager()->SetMockForDevice(device_info->guid, &mock_device);

  // Call GetDevices and expect the device to be returned.
  GetDevicesBlocking(service.get(), {device_info->guid});

  // Call GetDevice to get the device. The WebContents should not indicate we
  // are connected to a device since the device is not opened.
  mojo::Remote<device::mojom::UsbDevice> device;
  service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(web_contents()->IsConnectedToUsbDevice());

  // Open the device. Now the WebContents should indicate we are connected to a
  // USB device.
  EXPECT_CALL(mock_device, Open)
      .WillOnce(RunOnceCallback<0>(device::mojom::UsbOpenDeviceError::OK));
  TestFuture<device::mojom::UsbOpenDeviceError> open_future;
  device->Open(open_future.GetCallback());
  EXPECT_EQ(open_future.Get(), device::mojom::UsbOpenDeviceError::OK);
  EXPECT_TRUE(web_contents()->IsConnectedToUsbDevice());

  // Close the device and check that the WebContents no longer indicates we are
  // connected.
  EXPECT_CALL(mock_device, Close).WillOnce(RunOnceClosure<0>());
  base::RunLoop loop;
  device->Close(loop.QuitClosure());
  loop.Run();
  EXPECT_FALSE(web_contents()->IsConnectedToUsbDevice());
}

TEST_F(ChromeUsbDelegateTest, OpenAndDisconnectDevice) {
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));

  mojo::Remote<blink::mojom::WebUsbService> service;
  ConnectToService(service.BindNewPipeAndPassReceiver());

  // Connect a device and grant permission to access it.
  auto fake_device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  auto device_info = device_manager()->AddDevice(fake_device);
  GetChooserContext()->GrantDevicePermission(origin, *device_info);
  device::MockUsbMojoDevice mock_device;
  device_manager()->SetMockForDevice(device_info->guid, &mock_device);

  // Call GetDevices and expect the device to be returned.
  GetDevicesBlocking(service.get(), {device_info->guid});

  // Call GetDevice to get the device. The WebContents should not indicate we
  // are connected to a device since the device is not opened.
  mojo::Remote<device::mojom::UsbDevice> device;
  service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(web_contents()->IsConnectedToUsbDevice());

  // Open the device. Now the WebContents should indicate we are connected to a
  // USB device.
  EXPECT_CALL(mock_device, Open)
      .WillOnce(RunOnceCallback<0>(device::mojom::UsbOpenDeviceError::OK));
  TestFuture<device::mojom::UsbOpenDeviceError> open_future;
  device->Open(open_future.GetCallback());
  EXPECT_EQ(open_future.Get(), device::mojom::UsbOpenDeviceError::OK);
  EXPECT_TRUE(web_contents()->IsConnectedToUsbDevice());

  // Remove the device and check that the WebContents no longer indicates we are
  // connected.
  EXPECT_CALL(mock_device, Close).WillOnce(RunOnceClosure<0>());
  device_manager()->RemoveDevice(fake_device);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(web_contents()->IsConnectedToUsbDevice());
}

TEST_F(ChromeUsbDelegateTest, OpenAndNavigateCrossOrigin) {
  const auto origin = url::Origin::Create(GURL(kDefaultTestUrl));

  mojo::Remote<blink::mojom::WebUsbService> service;
  ConnectToService(service.BindNewPipeAndPassReceiver());

  // Connect a device and grant permission to access it.
  auto fake_device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
  auto device_info = device_manager()->AddDevice(fake_device);
  GetChooserContext()->GrantDevicePermission(origin, *device_info);
  device::MockUsbMojoDevice mock_device;
  device_manager()->SetMockForDevice(device_info->guid, &mock_device);

  // Call GetDevices and expect the device info to be returned.
  GetDevicesBlocking(service.get(), {device_info->guid});

  // Call GetDevice to get the device. The WebContents should not indicate we
  // are connected to a device since the device is not opened.
  mojo::Remote<device::mojom::UsbDevice> device;
  service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(web_contents()->IsConnectedToUsbDevice());

  // Open the device. Now the WebContents should indicate we are connected to a
  // USB device.
  EXPECT_CALL(mock_device, Open)
      .WillOnce(RunOnceCallback<0>(device::mojom::UsbOpenDeviceError::OK));
  TestFuture<device::mojom::UsbOpenDeviceError> open_future;
  device->Open(open_future.GetCallback());
  EXPECT_EQ(open_future.Get(), device::mojom::UsbOpenDeviceError::OK);
  EXPECT_TRUE(web_contents()->IsConnectedToUsbDevice());

  // Perform a cross-origin navigation. The WebContents should indicate we are
  // no longer connected.
  EXPECT_CALL(mock_device, Close).WillOnce(RunOnceClosure<0>());
  NavigateAndCommit(GURL(kCrossOriginTestUrl));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(web_contents()->IsConnectedToUsbDevice());
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(ChromeUsbDelegateTest, AllowlistedImprivataExtension) {
  absl::optional<GURL> imprivata_url =
      CreateExtensionWithId(kAllowlistedImprivataExtensionId);
  ASSERT_TRUE(imprivata_url);

  const auto imprivata_origin = url::Origin::Create(*imprivata_url);

  auto* context = GetChooserContext();

  auto device_info = device_manager()->AddDevice(CreateFakeHidDeviceInfo());
  context->GrantDevicePermission(imprivata_origin, *device_info);

  NavigateAndCommit(*imprivata_url);

  mojo::Remote<blink::mojom::WebUsbService> service;
  ConnectToService(service.BindNewPipeAndPassReceiver());

  GetDevicesBlocking(service.get(), {device_info->guid});

  mojo::Remote<device::mojom::UsbDevice> device;
  service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(web_contents()->IsConnectedToUsbDevice());

  TestFuture<device::mojom::UsbOpenDeviceError> open_future;
  device->Open(open_future.GetCallback());
  EXPECT_EQ(open_future.Get(), device::mojom::UsbOpenDeviceError::OK);
  EXPECT_TRUE(web_contents()->IsConnectedToUsbDevice());

  TestFuture<bool> set_configuration_future;
  device->SetConfiguration(1, set_configuration_future.GetCallback());
  EXPECT_TRUE(set_configuration_future.Get());

  TestFuture<device::mojom::UsbClaimInterfaceResult> claim_interface_future;
  device->ClaimInterface(0, claim_interface_future.GetCallback());

#if BUILDFLAG(IS_CHROMEOS)
  // The allowlist only allows the interface to be claimed on Chrome OS.
  EXPECT_EQ(claim_interface_future.Get(),
            device::mojom::UsbClaimInterfaceResult::kSuccess);
#else
  EXPECT_EQ(claim_interface_future.Get(),
            device::mojom::UsbClaimInterfaceResult::kProtectedClass);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_F(ChromeUsbDelegateTest, AllowlistedSmartCardConnectorExtension) {
  absl::optional<GURL> page_url =
      CreateExtensionWithId(kAllowlistedSmartCardExtensionId);
  ASSERT_TRUE(page_url);
  const auto extension_origin = url::Origin::Create(*page_url);

  // Add a smart card device. Also add an unrelated device, in order to test
  // that access is not automatically granted to it.
  auto ccid_device_info =
      device_manager()->AddDevice(CreateFakeSmartCardDeviceInfo());
  auto hid_device_info = device_manager()->AddDevice(CreateFakeHidDeviceInfo());

  // No need to grant permission. It is granted automatically for smart
  // card device.
  NavigateAndCommit(*page_url);

  mojo::Remote<blink::mojom::WebUsbService> service;
  ConnectToService(service.BindNewPipeAndPassReceiver());

  // Check that the extensions is automatically granted access to the CCID
  // device and can claim its interfaces.
  {
    GetDevicesBlocking(service.get(), {ccid_device_info->guid});

    mojo::Remote<device::mojom::UsbDevice> device;
    service->GetDevice(ccid_device_info->guid,
                       device.BindNewPipeAndPassReceiver());
    EXPECT_FALSE(web_contents()->IsConnectedToUsbDevice());

    TestFuture<device::mojom::UsbOpenDeviceError> open_future;
    device->Open(open_future.GetCallback());
    EXPECT_EQ(open_future.Get(), device::mojom::UsbOpenDeviceError::OK);
    EXPECT_TRUE(web_contents()->IsConnectedToUsbDevice());

    TestFuture<bool> set_configuration_future;
    device->SetConfiguration(1, set_configuration_future.GetCallback());
    EXPECT_TRUE(set_configuration_future.Get());

    TestFuture<device::mojom::UsbClaimInterfaceResult> claim_interface_future;
    device->ClaimInterface(0, claim_interface_future.GetCallback());
    EXPECT_EQ(claim_interface_future.Get(),
              device::mojom::UsbClaimInterfaceResult::kSuccess);
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

    TestFuture<device::mojom::UsbOpenDeviceError> open_future;
    device->Open(open_future.GetCallback());
    EXPECT_EQ(open_future.Get(), device::mojom::UsbOpenDeviceError::OK);

    TestFuture<bool> set_configuration_future;
    device->SetConfiguration(1, set_configuration_future.GetCallback());
    EXPECT_TRUE(set_configuration_future.Get());

    TestFuture<device::mojom::UsbClaimInterfaceResult> claim_interface_future;
    device->ClaimInterface(0, claim_interface_future.GetCallback());
    EXPECT_EQ(claim_interface_future.Get(),
              device::mojom::UsbClaimInterfaceResult::kProtectedClass);
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
