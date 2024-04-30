// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/chrome_usb_delegate.h"

#include <optional>
#include <string_view>

#include "base/barrier_closure.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_connection_tracker.h"
#include "chrome/browser/usb/usb_connection_tracker_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/embedded_worker_instance_test_harness.h"
#include "content/public/test/test_renderer_host.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/command_line.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

using ::base::test::RunClosure;
using ::base::test::RunOnceCallback;
using ::base::test::RunOnceClosure;
using ::base::test::TestFuture;
using ::testing::_;
using ::testing::NiceMock;

constexpr std::string_view kDefaultTestUrl{"https://www.google.com/"};
constexpr std::string_view kCrossOriginTestUrl{"https://www.chromium.org"};

#if BUILDFLAG(ENABLE_EXTENSIONS)
constexpr std::string_view kExtensionId{"ckcendljdlmgnhghiaomidhiiclmapok"};
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

device::mojom::UsbOpenDeviceResultPtr NewUsbOpenDeviceSuccess() {
  return device::mojom::UsbOpenDeviceResult::NewSuccess(
      device::mojom::UsbOpenDeviceSuccess::OK);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Creates a FakeUsbDeviceInfo with USB class code.
scoped_refptr<device::FakeUsbDeviceInfo> CreateFakeUsbDeviceInfo() {
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

class MockUsbConnectionTracker : public UsbConnectionTracker {
 public:
  explicit MockUsbConnectionTracker(Profile* profile)
      : UsbConnectionTracker(profile) {}
  ~MockUsbConnectionTracker() override = default;

  MOCK_METHOD(void, IncrementConnectionCount, (const url::Origin&), (override));
  MOCK_METHOD(void, DecrementConnectionCount, (const url::Origin&), (override));
};

// Tests for embedder-specific behaviors of Chrome's blink::mojom::WebUsbService
// implementation.
class ChromeUsbTestHelper {
 public:
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Creates a fake extension with the specified `extension_id` so that it can
  // exercise behaviors that are only enabled for privileged extensions.
  std::optional<GURL> CreateExtensionWithId(std::string_view extension_id) {
    auto manifest = base::Value::Dict()
                        .Set("name", "Fake extension")
                        .Set("description", "For testing.")
                        .Set("version", "0.1")
                        .Set("manifest_version", 2)
                        .Set("web_accessible_resources",
                             base::Value::List().Append("index.html"));
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder()
            .SetManifest(std::move(manifest))
            .SetID(std::string(extension_id))
            .Build();
    if (!extension) {
      return std::nullopt;
    }
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_));
    extensions::ExtensionService* extension_service =
        extension_system->CreateExtensionService(
            base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service->AddExtension(extension.get());
    return extension->GetResourceURL("index.html");
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  void SimulateDeviceServiceCrash() { device_manager()->CloseAllBindings(); }

  virtual void ConnectToService(
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) = 0;

  virtual void SetUpOriginUrl() = 0;

  void SetUpWebPageOriginUrl() { origin_url_ = GURL(kDefaultTestUrl); }

  void SetUpExtensionOriginUrl(std::string_view extension_id) {
    auto extension_url = CreateExtensionWithId(extension_id);
    ASSERT_TRUE(extension_url);
    origin_url_ = *extension_url;
  }

  void SetUpFakeDeviceManager() {
    if (!device_manager()->IsBound()) {
      mojo::PendingRemote<device::mojom::UsbDeviceManager>
          pending_device_manager;
      device_manager()->AddReceiver(
          pending_device_manager.InitWithNewPipeAndPassReceiver());
      GetChooserContext()->SetDeviceManagerForTesting(
          std::move(pending_device_manager));
    }
  }

  UsbChooserContext* GetChooserContext() {
    return UsbChooserContextFactory::GetForProfile(profile_);
  }

  device::FakeUsbDeviceManager* device_manager() {
    if (!device_manager_) {
      device_manager_ = std::make_unique<device::FakeUsbDeviceManager>();
    }
    return device_manager_.get();
  }

  BrowserContextKeyedServiceFactory::TestingFactory
  GetUsbConnectionTrackerTestingFactory() {
    return base::BindRepeating([](content::BrowserContext* browser_context) {
      return static_cast<std::unique_ptr<KeyedService>>(
          std::make_unique<testing::NiceMock<MockUsbConnectionTracker>>(
              Profile::FromBrowserContext(browser_context)));
    });
  }

  void SetUpUsbConnectionTracker() {
    // Even MockUsbConnectionTracker can be lazily created in ChromeUsbDelegate,
    // we intentionally create it ahead of time so that we can test EXPECT_CALL
    // for invoking mock method for the first time.
    usb_connection_tracker_ = static_cast<MockUsbConnectionTracker*>(
        UsbConnectionTrackerFactory::GetForProfile(profile_, /*create=*/true));
  }

  MockUsbConnectionTracker& usb_connection_tracker() {
    return *usb_connection_tracker_;
  }

  void TestNoPermissionDevice() {
    auto origin = url::Origin::Create(origin_url_);
    auto device1 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
        0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
    auto device2 = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
        0x1234, 0x5679, "ACME", "Frobinator+", "GHIJKL");
    auto no_permission_device1 =
        base::MakeRefCounted<device::FakeUsbDeviceInfo>(
            0xffff, 0x567b, "ACME", "Frobinator II", "MNOPQR");
    auto no_permission_device2 =
        base::MakeRefCounted<device::FakeUsbDeviceInfo>(
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

    // Disconnect all four devices. The `mock_client` should be notified only
    // for the devices it has permission to access.
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

  void TestReconnectDeviceManager() {
    auto origin = url::Origin::Create(origin_url_);
    auto* context = GetChooserContext();
    auto device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
        0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
    auto ephemeral_device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
        0, 0, "ACME", "Frobinator II", "");

    // Connect two devices and grant permission for both. The first device is
    // eligible for persistent permissions and the second device is only
    // eligible for ephemeral permissions.
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
    EXPECT_CALL(mock_client, ConnectionError()).WillOnce([&]() {
      loop.Quit();
    });
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

  void TestRevokeDevicePermission() {
    auto origin = url::Origin::Create(origin_url_);
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

  void TestOpenAndCloseDevice(content::WebContents* web_contents) {
    auto origin = url::Origin::Create(origin_url_);
    mojo::Remote<blink::mojom::WebUsbService> service;
    ConnectToService(service.BindNewPipeAndPassReceiver());

    // Connect a device and grant permission to access it.
    auto device_info = device_manager()->CreateAndAddDevice(
        0x1234, 0x5678, "ACME", "Frobinator", "ABCDEF");
    GetChooserContext()->GrantDevicePermission(origin, *device_info);
    device::MockUsbMojoDevice mock_device;
    device_manager()->SetMockForDevice(device_info->guid, &mock_device);

    // Call GetDevices and expect the device to be returned.
    MockDeviceManagerClient mock_client;
    service->SetClient(mock_client.CreateInterfacePtrAndBind());
    GetDevicesBlocking(service.get(), {device_info->guid});

    // Call GetDevice to get the device. The WebContents should not indicate we
    // are connected to a device since the device is not opened.
    mojo::Remote<device::mojom::UsbDevice> device;
    service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
    if (web_contents) {
      EXPECT_FALSE(web_contents->IsConnectedToUsbDevice());
    }

    // Open the device. Now the WebContents should indicate we are connected to
    // a USB device.
    EXPECT_CALL(mock_device, Open)
        .WillOnce(RunOnceCallback<0>(NewUsbOpenDeviceSuccess()));
    TestFuture<device::mojom::UsbOpenDeviceResultPtr> open_future;
    if (supports_usb_connection_tracker_) {
      EXPECT_CALL(usb_connection_tracker(), IncrementConnectionCount(origin));
    }
    device->Open(open_future.GetCallback());
    EXPECT_TRUE(open_future.Get()->is_success());
    if (web_contents) {
      EXPECT_TRUE(web_contents->IsConnectedToUsbDevice());
    }

    // Close the device and check that the WebContents no longer indicates we
    // are connected.
    EXPECT_CALL(mock_device, Close).WillOnce(RunOnceClosure<0>());
    base::RunLoop loop;
    if (supports_usb_connection_tracker_) {
      EXPECT_CALL(usb_connection_tracker(), DecrementConnectionCount(origin));
    }
    device->Close(loop.QuitClosure());
    loop.Run();
    if (web_contents) {
      EXPECT_FALSE(web_contents->IsConnectedToUsbDevice());
    }
  }

  void TestOpenAndDisconnectDevice(content::WebContents* web_contents) {
    auto origin = url::Origin::Create(origin_url_);
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
    MockDeviceManagerClient mock_client;
    service->SetClient(mock_client.CreateInterfacePtrAndBind());
    GetDevicesBlocking(service.get(), {device_info->guid});

    // Call GetDevice to get the device. The WebContents should not indicate we
    // are connected to a device since the device is not opened.
    mojo::Remote<device::mojom::UsbDevice> device;
    service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
    if (web_contents) {
      EXPECT_FALSE(web_contents->IsConnectedToUsbDevice());
    }

    // Open the device. Now the WebContents should indicate we are connected to
    // a USB device.
    EXPECT_CALL(mock_device, Open)
        .WillOnce(RunOnceCallback<0>(NewUsbOpenDeviceSuccess()));
    TestFuture<device::mojom::UsbOpenDeviceResultPtr> open_future;
    if (supports_usb_connection_tracker_) {
      EXPECT_CALL(usb_connection_tracker(), IncrementConnectionCount(origin));
    }
    device->Open(open_future.GetCallback());
    EXPECT_TRUE(open_future.Get()->is_success());
    if (web_contents) {
      EXPECT_TRUE(web_contents->IsConnectedToUsbDevice());
    }

    // Remove the device and check that the WebContents no longer indicates we
    // are connected.
    base::RunLoop decrement_connection_count_loop;
    if (supports_usb_connection_tracker_) {
      EXPECT_CALL(usb_connection_tracker(), DecrementConnectionCount(origin))
          .WillOnce(RunClosure(decrement_connection_count_loop.QuitClosure()));
    }
    EXPECT_CALL(mock_device, Close).WillOnce(RunOnceClosure<0>());
    device_manager()->RemoveDevice(fake_device);
    if (supports_usb_connection_tracker_) {
      decrement_connection_count_loop.Run();
    } else {
      base::RunLoop().RunUntilIdle();
    }
    if (web_contents) {
      EXPECT_FALSE(web_contents->IsConnectedToUsbDevice());
    }
  }

  void TestWebUsbServiceNotConnected() {
    base::RunLoop run_loop;
    mojo::Remote<blink::mojom::WebUsbService> web_usb_service;
    ConnectToService(web_usb_service.BindNewPipeAndPassReceiver());
    web_usb_service.set_disconnect_handler(run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_FALSE(web_usb_service.is_connected());
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  void TestAllowlistedImprivataExtension(content::WebContents* web_contents) {
    auto imprivata_origin = url::Origin::Create(origin_url_);
    auto* context = GetChooserContext();
    auto device_info = device_manager()->AddDevice(CreateFakeUsbDeviceInfo());
    context->GrantDevicePermission(imprivata_origin, *device_info);

    mojo::Remote<blink::mojom::WebUsbService> service;
    ConnectToService(service.BindNewPipeAndPassReceiver());
    MockDeviceManagerClient mock_client;
    service->SetClient(mock_client.CreateInterfacePtrAndBind());
    GetDevicesBlocking(service.get(), {device_info->guid});

    mojo::Remote<device::mojom::UsbDevice> device;
    service->GetDevice(device_info->guid, device.BindNewPipeAndPassReceiver());
    if (web_contents) {
      EXPECT_FALSE(web_contents->IsConnectedToUsbDevice());
    }

    TestFuture<device::mojom::UsbOpenDeviceResultPtr> open_future;
    device->Open(open_future.GetCallback());
    EXPECT_TRUE(open_future.Get()->is_success());
    if (web_contents) {
      EXPECT_TRUE(web_contents->IsConnectedToUsbDevice());
    }

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

  void TestAllowlistedSmartCardConnectorExtension(
      content::WebContents* web_contents) {
    auto extension_origin = url::Origin::Create(origin_url_);
    // Add a smart card device. Also add an unrelated device, in order to test
    // that access is not automatically granted to it.
    auto ccid_device_info =
        device_manager()->AddDevice(CreateFakeSmartCardDeviceInfo());
    auto usb_device_info =
        device_manager()->AddDevice(CreateFakeUsbDeviceInfo());

    // No need to grant permission. It is granted automatically for smart
    // card device.

    mojo::Remote<blink::mojom::WebUsbService> service;
    ConnectToService(service.BindNewPipeAndPassReceiver());
    MockDeviceManagerClient mock_client;
    service->SetClient(mock_client.CreateInterfacePtrAndBind());

    // Check that the extensions is automatically granted access to the CCID
    // device and can claim its interfaces.
    {
      GetDevicesBlocking(service.get(), {ccid_device_info->guid});

      mojo::Remote<device::mojom::UsbDevice> device;
      service->GetDevice(ccid_device_info->guid,
                         device.BindNewPipeAndPassReceiver());
      if (web_contents) {
        EXPECT_FALSE(web_contents->IsConnectedToUsbDevice());
      }

      TestFuture<device::mojom::UsbOpenDeviceResultPtr> open_future;
      device->Open(open_future.GetCallback());
      EXPECT_TRUE(open_future.Get()->is_success());
      if (web_contents) {
        EXPECT_TRUE(web_contents->IsConnectedToUsbDevice());
      }

      TestFuture<bool> set_configuration_future;
      device->SetConfiguration(1, set_configuration_future.GetCallback());
      EXPECT_TRUE(set_configuration_future.Get());

      TestFuture<device::mojom::UsbClaimInterfaceResult> claim_interface_future;
      device->ClaimInterface(0, claim_interface_future.GetCallback());
      EXPECT_EQ(claim_interface_future.Get(),
                device::mojom::UsbClaimInterfaceResult::kSuccess);
    }

    // Check that the extension, if granted permission to a USB device can't
    // claim its interfaces.
    {
      GetChooserContext()->GrantDevicePermission(extension_origin,
                                                 *usb_device_info);
      GetDevicesBlocking(service.get(),
                         {ccid_device_info->guid, usb_device_info->guid});

      mojo::Remote<device::mojom::UsbDevice> device;
      service->GetDevice(usb_device_info->guid,
                         device.BindNewPipeAndPassReceiver());

      TestFuture<device::mojom::UsbOpenDeviceResultPtr> open_future;
      device->Open(open_future.GetCallback());
      EXPECT_TRUE(open_future.Get()->is_success());

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

 protected:
  raw_ptr<TestingProfile, DanglingUntriaged> profile_ = nullptr;
  GURL origin_url_;
  raw_ptr<MockUsbConnectionTracker, DanglingUntriaged> usb_connection_tracker_ =
      nullptr;
  // This flag is expected to be set to true only for the scenario of extension
  // origin and kEnableWebUsbOnExtensionServiceWorker enabled.
  bool supports_usb_connection_tracker_ = false;

 private:
  std::unique_ptr<device::FakeUsbDeviceManager> device_manager_;
};

class ChromeUsbDelegateRenderFrameTestBase
    : public ChromeRenderViewHostTestHarness,
      public ChromeUsbTestHelper {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile_ = profile();
    ASSERT_TRUE(profile_);
    UsbConnectionTrackerFactory::GetInstance()->SetTestingFactory(
        profile_, GetUsbConnectionTrackerTestingFactory());
    SetUpUsbConnectionTracker();
    SetUpOriginUrl();
    NavigateAndCommit(origin_url_);
  }

  // ChromeUsbTestHelper:
  void ConnectToService(
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) override {
    SetUpFakeDeviceManager();
    content::RenderFrameHostTester::For(main_rfh())
        ->CreateWebUsbServiceForTesting(std::move(receiver));
  }

  void TestOpenAndNavigateCrossOrigin(content::WebContents* web_contents) {
    auto origin = url::Origin::Create(origin_url_);
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
    if (web_contents) {
      EXPECT_FALSE(web_contents->IsConnectedToUsbDevice());
    }

    // Open the device. Now the WebContents should indicate we are connected to
    // a USB device.
    EXPECT_CALL(mock_device, Open)
        .WillOnce(RunOnceCallback<0>(NewUsbOpenDeviceSuccess()));
    TestFuture<device::mojom::UsbOpenDeviceResultPtr> open_future;
    device->Open(open_future.GetCallback());
    EXPECT_TRUE(open_future.Get()->is_success());
    if (web_contents) {
      EXPECT_TRUE(web_contents->IsConnectedToUsbDevice());
    }

    // Perform a cross-origin navigation. The WebContents should indicate we are
    // no longer connected.
    EXPECT_CALL(mock_device, Close).WillOnce(RunOnceClosure<0>());
    NavigateAndCommit(GURL(kCrossOriginTestUrl));
    base::RunLoop().RunUntilIdle();
    if (web_contents) {
      EXPECT_FALSE(web_contents->IsConnectedToUsbDevice());
    }
  }
};

class ChromeUsbDelegateServiceWorkerTestBase
    : public content::EmbeddedWorkerInstanceTestHarness,
      public ChromeUsbTestHelper {
 public:
  void SetUp() override {
    content::EmbeddedWorkerInstanceTestHarness::SetUp();
    SetUpOriginUrl();
    SetUpUsbConnectionTracker();
    StartWorker();
  }

  void TearDown() override {
    StopWorker();
    content::EmbeddedWorkerInstanceTestHarness::TearDown();
  }

  // ChromeUsbTestHelper:
  void ConnectToService(
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) override {
    SetUpFakeDeviceManager();
    BindUsbServiceToWorker(origin_url_, std::move(receiver));
  }

  // content::EmbeddedWorkerInstanceTestHarness
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    auto builder = TestingProfile::Builder();
    auto testing_profile = builder.Build();
    profile_ = testing_profile.get();
    UsbConnectionTrackerFactory::GetInstance()->SetTestingFactory(
        profile_, GetUsbConnectionTrackerTestingFactory());
    return testing_profile;
  }

  void StartWorker() {
    auto worker_url =
        GURL(base::StringPrintf("%s/worker.js", origin_url_.spec().c_str()));
    CreateAndStartWorker(origin_url_, worker_url);

    // Wait until tasks triggered by ServiceWorkerUsbDelegateObserver settle.
    base::RunLoop().RunUntilIdle();
  }

  void StopWorker() { StopAndResetWorker(); }
};

class ChromeUsbDelegateRenderFrameTest
    : public ChromeUsbDelegateRenderFrameTestBase {
 public:
  void SetUpOriginUrl() override { SetUpWebPageOriginUrl(); }
};

class ChromeUsbDelegateServiceWorkerTest
    : public ChromeUsbDelegateServiceWorkerTestBase {
 public:
  void SetUpOriginUrl() override { SetUpWebPageOriginUrl(); }
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
class DisableWebUsbOnExtensionServiceWorkerHelper {
 public:
  DisableWebUsbOnExtensionServiceWorkerHelper() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            features::kEnableWebUsbOnExtensionServiceWorker});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ChromeUsbDelegateExtensionRenderFrameTest
    : public ChromeUsbDelegateRenderFrameTestBase {
 public:
  ChromeUsbDelegateExtensionRenderFrameTest() {
    supports_usb_connection_tracker_ = true;
  }
  void SetUpOriginUrl() override { SetUpExtensionOriginUrl(kExtensionId); }
};

class ChromeUsbDelegateImprivataExtensionRenderFrameTest
    : public ChromeUsbDelegateRenderFrameTestBase {
 public:
  ChromeUsbDelegateImprivataExtensionRenderFrameTest() {
    supports_usb_connection_tracker_ = true;
  }
  void SetUpOriginUrl() override {
    SetUpExtensionOriginUrl(kAllowlistedImprivataExtensionId);
  }
};

class ChromeUsbDelegateSmartCardExtensionRenderFrameTest
    : public ChromeUsbDelegateRenderFrameTestBase {
 public:
  ChromeUsbDelegateSmartCardExtensionRenderFrameTest() {
    supports_usb_connection_tracker_ = true;
  }
  void SetUpOriginUrl() override {
    SetUpExtensionOriginUrl(kAllowlistedSmartCardExtensionId);
  }
};

class ChromeUsbDelegateExtensionRenderFrameFeatureDisabledTest
    : public ChromeUsbDelegateRenderFrameTestBase,
      public DisableWebUsbOnExtensionServiceWorkerHelper {
 public:
  ChromeUsbDelegateExtensionRenderFrameFeatureDisabledTest() {
    // There is no usb connection tracker activity when
    // features::kEnableWebUsbOnExtensionServiceWorker is disabled.
    supports_usb_connection_tracker_ = false;
  }
  void SetUpOriginUrl() override { SetUpExtensionOriginUrl(kExtensionId); }
};

class ChromeUsbDelegateExtensionServiceWorkerTest
    : public ChromeUsbDelegateServiceWorkerTestBase {
 public:
  ChromeUsbDelegateExtensionServiceWorkerTest() {
    supports_usb_connection_tracker_ = true;
  }
  void SetUpOriginUrl() override { SetUpExtensionOriginUrl(kExtensionId); }
};

class ChromeUsbDelegateExtensionServiceWorkerFeatureDisabledTest
    : public ChromeUsbDelegateServiceWorkerTestBase,
      public DisableWebUsbOnExtensionServiceWorkerHelper {
 public:
  ChromeUsbDelegateExtensionServiceWorkerFeatureDisabledTest() {
    // There is no usb connection tracker activity when
    // features::kEnableWebUsbOnExtensionServiceWorker is disabled.
    supports_usb_connection_tracker_ = false;
  }
  void SetUpOriginUrl() override { SetUpExtensionOriginUrl(kExtensionId); }
};

class ChromeUsbDelegateImprivataExtensionServiceWorkerTest
    : public ChromeUsbDelegateServiceWorkerTestBase {
 public:
  ChromeUsbDelegateImprivataExtensionServiceWorkerTest() {
    supports_usb_connection_tracker_ = true;
  }
  void SetUpOriginUrl() override {
    SetUpExtensionOriginUrl(kAllowlistedImprivataExtensionId);
  }
};

class ChromeUsbDelegateSmartCardExtensionServiceWorkerTest
    : public ChromeUsbDelegateServiceWorkerTestBase {
 public:
  ChromeUsbDelegateSmartCardExtensionServiceWorkerTest() {
    supports_usb_connection_tracker_ = true;
  }
  void SetUpOriginUrl() override {
    SetUpExtensionOriginUrl(kAllowlistedSmartCardExtensionId);
  }
};

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

TEST_F(ChromeUsbDelegateRenderFrameTest, NoPermissionDevice) {
  TestNoPermissionDevice();
}

TEST_F(ChromeUsbDelegateRenderFrameTest, ReconnectDeviceManager) {
  TestReconnectDeviceManager();
}

TEST_F(ChromeUsbDelegateRenderFrameTest, RevokeDevicePermission) {
  TestRevokeDevicePermission();
}

TEST_F(ChromeUsbDelegateRenderFrameTest, OpenAndCloseDevice) {
  TestOpenAndCloseDevice(web_contents());
}

TEST_F(ChromeUsbDelegateRenderFrameTest, OpenAndDisconnectDevice) {
  TestOpenAndDisconnectDevice(web_contents());
}

TEST_F(ChromeUsbDelegateRenderFrameTest, OpenAndNavigateCrossOrigin) {
  TestOpenAndNavigateCrossOrigin(web_contents());
}

TEST_F(ChromeUsbDelegateServiceWorkerTest, WebUsbServiceNotConnected) {
  TestWebUsbServiceNotConnected();
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(ChromeUsbDelegateExtensionRenderFrameTest, NoPermissionDevice) {
  TestNoPermissionDevice();
}

TEST_F(ChromeUsbDelegateExtensionRenderFrameTest, ReconnectDeviceManager) {
  TestReconnectDeviceManager();
}

TEST_F(ChromeUsbDelegateExtensionRenderFrameTest, RevokeDevicePermission) {
  TestRevokeDevicePermission();
}

TEST_F(ChromeUsbDelegateExtensionRenderFrameTest, OpenAndCloseDevice) {
  TestOpenAndCloseDevice(web_contents());
}

TEST_F(ChromeUsbDelegateExtensionRenderFrameTest, OpenAndDisconnectDevice) {
  TestOpenAndDisconnectDevice(web_contents());
}

TEST_F(ChromeUsbDelegateImprivataExtensionRenderFrameTest,
       AllowlistedImprivataExtension) {
  TestAllowlistedImprivataExtension(web_contents());
}

TEST_F(ChromeUsbDelegateSmartCardExtensionRenderFrameTest,
       AllowlistedSmartCardConnectorExtension) {
  TestAllowlistedSmartCardConnectorExtension(web_contents());
}

TEST_F(ChromeUsbDelegateExtensionRenderFrameFeatureDisabledTest,
       OpenAndCloseDevice) {
  TestOpenAndCloseDevice(web_contents());
}

TEST_F(ChromeUsbDelegateExtensionRenderFrameFeatureDisabledTest,
       OpenAndDisconnectDevice) {
  TestOpenAndDisconnectDevice(web_contents());
}

TEST_F(ChromeUsbDelegateExtensionServiceWorkerFeatureDisabledTest,
       WebUsbServiceNotConnected) {
  TestWebUsbServiceNotConnected();
}

TEST_F(ChromeUsbDelegateImprivataExtensionServiceWorkerTest,
       AllowlistedImprivataExtension) {
  TestAllowlistedImprivataExtension(nullptr);
}

TEST_F(ChromeUsbDelegateSmartCardExtensionServiceWorkerTest,
       AllowlistedSmartCardConnectorExtension) {
  TestAllowlistedSmartCardConnectorExtension(nullptr);
}

TEST_F(ChromeUsbDelegateExtensionServiceWorkerTest, NoPermissionDevice) {
  TestNoPermissionDevice();
}

TEST_F(ChromeUsbDelegateExtensionServiceWorkerTest, ReconnectDeviceManager) {
  TestReconnectDeviceManager();
}

TEST_F(ChromeUsbDelegateExtensionServiceWorkerTest, RevokeDevicePermission) {
  TestRevokeDevicePermission();
}

TEST_F(ChromeUsbDelegateExtensionServiceWorkerTest, OpenAndCloseDevice) {
  TestOpenAndCloseDevice(/*web_contents=*/nullptr);
}

TEST_F(ChromeUsbDelegateExtensionServiceWorkerTest, OpenAndDisconnectDevice) {
  TestOpenAndDisconnectDevice(/*web_contents=*/nullptr);
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

TEST(ChromeUsbDelegateBrowserContextTest, BrowserContextIsNull) {
  base::test::SingleThreadTaskEnvironment task_environment;
  ChromeUsbDelegate chrome_usb_delegate;
  url::Origin origin = url::Origin::Create(GURL(kDefaultTestUrl));
  EXPECT_FALSE(chrome_usb_delegate.CanRequestDevicePermission(
      /*browser_context=*/nullptr, origin));
  EXPECT_FALSE(chrome_usb_delegate.HasDevicePermission(
      /*browser_context=*/nullptr, /*frame=*/nullptr, origin,
      device::mojom::UsbDeviceInfo()));
  EXPECT_EQ(nullptr, chrome_usb_delegate.GetDeviceInfo(
                         /*browser_context=*/nullptr,
                         base::Uuid::GenerateRandomV4().AsLowercaseString()));

  TestFuture<std::vector<device::mojom::UsbDeviceInfoPtr>> get_devices_future;
  chrome_usb_delegate.GetDevices(/*browser_context=*/nullptr,
                                 get_devices_future.GetCallback());
  EXPECT_TRUE(get_devices_future.Get().empty());

  // TODO(crbug.com/40217296): Test GetDevice with null browser_context.
}
