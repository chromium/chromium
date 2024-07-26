// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/serial/serial_chooser_controller.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "chrome/browser/serial/serial_blocklist.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/serial/serial_chooser_histograms.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/permissions/mock_chooser_controller_view.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"
#include "services/device/public/cpp/test/fake_serial_port_manager.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/serial/serial.mojom.h"

namespace {

using ::device::BluetoothUUID;
using ::device::MockBluetoothAdapter;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

constexpr char kBluetoothDevice1Address[] = "00:11:22:33:44:55";
constexpr char16_t kBluetoothDevice1Name[] = u"Bluetooth #1";
constexpr char kBluetoothDevice2Address[] = "11:22:33:44:55:66";
constexpr char16_t kBluetoothDevice2Name[] = u"Bluetooth #2";
const BluetoothUUID kRandomBluetoothServiceClassId(
    "34a0fe08-1c1f-4251-879e-2a8c397e56ee");

device::mojom::SerialPortInfoPtr CreateBluetoothPort(
    const std::string& device_address,
    const std::u16string& device_name,
    const BluetoothUUID& service_class_id) {
  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port->path = base::FilePath::FromUTF8Unsafe(device_address);
  port->type = device::mojom::SerialPortType::BLUETOOTH_CLASSIC_RFCOMM;
  port->bluetooth_service_class_id = service_class_id;
  port->display_name = base::UTF16ToUTF8(device_name);
  return port;
}

}  // namespace

class SerialChooserControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    mojo::PendingRemote<device::mojom::SerialPortManager> port_manager;
    port_manager_.AddReceiver(port_manager.InitWithNewPipeAndPassReceiver());
    SerialChooserContextFactory::GetForProfile(profile())
        ->SetPortManagerForTesting(std::move(port_manager));

    adapter_ = base::MakeRefCounted<testing::NiceMock<MockBluetoothAdapter>>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
    ON_CALL(*adapter_, GetOsPermissionStatus)
        .WillByDefault(
            Return(device::BluetoothAdapter::PermissionStatus::kAllowed));
    ON_CALL(*adapter_, IsPowered).WillByDefault(Return(true));
  }

  base::UnguessableToken AddBluetoothPort(
      const std::string& device_address,
      const std::u16string& device_name,
      const BluetoothUUID& service_class_id) {
    auto port =
        CreateBluetoothPort(device_address, device_name, service_class_id);

    base::UnguessableToken port_token = port->token;
    port_manager().AddPort(std::move(port));
    return port_token;
  }

  base::UnguessableToken AddPort(
      const std::string& display_name,
      const base::FilePath& path,
      std::optional<uint16_t> vendor_id = std::nullopt,
      std::optional<uint16_t> product_id = std::nullopt) {
    auto port = device::mojom::SerialPortInfo::New();
    port->token = base::UnguessableToken::Create();
    port->display_name = display_name;
    port->path = path;
    if (vendor_id) {
      port->has_vendor_id = true;
      port->vendor_id = *vendor_id;
    }
    if (product_id) {
      port->has_product_id = true;
      port->product_id = *product_id;
    }
    base::UnguessableToken port_token = port->token;
    port_manager().AddPort(std::move(port));
    return port_token;
  }

  device::FakeSerialPortManager& port_manager() { return port_manager_; }
  MockBluetoothAdapter* adapter() { return adapter_.get(); }

 private:
  device::FakeSerialPortManager port_manager_;
  scoped_refptr<MockBluetoothAdapter> adapter_;
};

TEST_F(SerialChooserControllerTest, GetPortsLateResponse) {
  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  std::vector<device::BluetoothUUID> allowed_bluetooth_service_class_ids;

  bool callback_run = false;
  auto callback = base::BindLambdaForTesting(
      [&](device::mojom::SerialPortInfoPtr port_info) {
        EXPECT_FALSE(port_info);
        callback_run = true;
      });

  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters),
      std::move(allowed_bluetooth_service_class_ids), std::move(callback));
  controller.reset();

  // Allow any tasks posted by |controller| to run, such as asynchronous
  // requests to the Device Service to get the list of available serial ports.
  // These should be safely discarded since |controller| was destroyed.
  base::RunLoop().RunUntilIdle();

  // Even if |controller| is destroyed without user interaction the callback
  // should be run.
  EXPECT_TRUE(callback_run);
}

TEST_F(SerialChooserControllerTest, PortsAddedAndRemoved) {
  base::HistogramTester histogram_tester;

  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  std::vector<device::BluetoothUUID> allowed_bluetooth_service_class_ids;
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters),
      std::move(allowed_bluetooth_service_class_ids), base::DoNothing());

  permissions::MockChooserControllerView view;
  controller->set_view(&view);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
      run_loop.Quit();
    }));
    run_loop.Run();
  }
  EXPECT_EQ(0u, controller->NumOptions());

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port->display_name = "Test Port 1";
  port->path = base::FilePath(FILE_PATH_LITERAL("/dev/ttyS0"));
#if BUILDFLAG(IS_MAC)
  // This path will be ignored and not generate additional chooser entries or
  // be displayed in the device name.
  port->alternate_path = base::FilePath(FILE_PATH_LITERAL("/dev/alternateS0"));
#endif
  base::UnguessableToken port1_token = port->token;
  port_manager().AddPort(std::move(port));
  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionAdded(_)).WillOnce(Invoke([&](size_t index) {
      EXPECT_EQ(0u, index);
      run_loop.Quit();
    }));
    run_loop.Run();
  }
  EXPECT_EQ(1u, controller->NumOptions());
  EXPECT_EQ(u"Test Port 1 (ttyS0)", controller->GetOption(0));

  AddPort("Test Port 2", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS1")));
  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionAdded(_)).WillOnce(Invoke([&](size_t index) {
      EXPECT_EQ(1u, index);
      run_loop.Quit();
    }));
    run_loop.Run();
  }
  EXPECT_EQ(2u, controller->NumOptions());
  EXPECT_EQ(u"Test Port 1 (ttyS0)", controller->GetOption(0));
  EXPECT_EQ(u"Test Port 2 (ttyS1)", controller->GetOption(1));

  port_manager().RemovePort(port1_token);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionRemoved(_)).WillOnce(Invoke([&](size_t index) {
      EXPECT_EQ(0u, index);
      run_loop.Quit();
    }));
    run_loop.Run();
  }
  EXPECT_EQ(1u, controller->NumOptions());
  EXPECT_EQ(u"Test Port 2 (ttyS1)", controller->GetOption(0));

  controller.reset();
  histogram_tester.ExpectUniqueSample("Permissions.Serial.ChooserClosed",
                                      SerialChooserOutcome::kCancelled, 1);
}

TEST_F(SerialChooserControllerTest, PortSelected) {
  base::HistogramTester histogram_tester;

  base::UnguessableToken port_token =
      AddPort("Test Port", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS0")));

  base::MockCallback<content::SerialChooser::Callback> callback;
  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  std::vector<device::BluetoothUUID> allowed_bluetooth_service_class_ids;
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters),
      std::move(allowed_bluetooth_service_class_ids), callback.Get());

  permissions::MockChooserControllerView view;
  controller->set_view(&view);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
      EXPECT_EQ(1u, controller->NumOptions());
      EXPECT_EQ(u"Test Port (ttyS0)", controller->GetOption(0));
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  EXPECT_CALL(callback, Run(_))
      .WillOnce(Invoke([&](device::mojom::SerialPortInfoPtr port) {
        EXPECT_EQ(port_token, port->token);

        // Regression test for https://crbug.com/1069057. Ensure that the set of
        // options is still valid after the callback is run.
        EXPECT_EQ(1u, controller->NumOptions());
        EXPECT_EQ(u"Test Port (ttyS0)", controller->GetOption(0));
      }));
  controller->Select({0});
  histogram_tester.ExpectUniqueSample(
      "Permissions.Serial.ChooserClosed",
      SerialChooserOutcome::kEphemeralPermissionGranted, 1);
}

TEST_F(SerialChooserControllerTest, PortFiltered) {
  base::HistogramTester histogram_tester;

  // Create two ports from the same vendor with different product IDs.
  base::UnguessableToken port_1 =
      AddPort("Test Port 1", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS0")),
              0x1234, 0x0001);
  base::UnguessableToken port_2 =
      AddPort("Test Port 2", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS1")),
              0x1234, 0x0002);
  // and a Bluetooth port which should always be ignored for this test.
  AddBluetoothPort(kBluetoothDevice1Address, kBluetoothDevice1Name,
                   device::GetSerialPortProfileUUID());

  // Create a filter which will select only the first port.
  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  auto filter = blink::mojom::SerialPortFilter::New();
  filter->has_vendor_id = true;
  filter->vendor_id = 0x1234;
  filter->has_product_id = true;
  filter->product_id = 0x0001;
  filters.push_back(std::move(filter));

  std::vector<device::BluetoothUUID> allowed_bluetooth_service_class_ids;
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters),
      std::move(allowed_bluetooth_service_class_ids), base::DoNothing());

  permissions::MockChooserControllerView view;
  controller->set_view(&view);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
      // Expect that only the first port is shown thanks to the filter.
      EXPECT_EQ(1u, controller->NumOptions());
      EXPECT_EQ(u"Test Port 1 (ttyS0)", controller->GetOption(0));
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  // Removing the second port should be a no-op since it is filtered out.
  EXPECT_CALL(view, OnOptionRemoved).Times(0);
  port_manager().RemovePort(port_2);
  base::RunLoop().RunUntilIdle();

  // Adding it back should be a no-op as well.
  EXPECT_CALL(view, OnOptionAdded).Times(0);
  AddPort("Test Port 2", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS1")),
          0x1234, 0x0002);
  base::RunLoop().RunUntilIdle();

  // Removing the first port should trigger a change in the UI. This also acts
  // as a synchronization point to make sure that the changes above were
  // processed.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionRemoved(0)).WillOnce(Invoke([&]() {
      run_loop.Quit();
    }));
    port_manager().RemovePort(port_1);
    run_loop.Run();
  }
}

TEST_F(SerialChooserControllerTest, BluetoothPortFiltered) {
  base::HistogramTester histogram_tester;

  // Create a wired and a Bluetooth port.
  base::UnguessableToken port_1 =
      AddPort("Test Port 1", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS0")),
              0x1234, 0x0001);
  base::UnguessableToken bluetooth_port_token =
      AddBluetoothPort(kBluetoothDevice1Address, kBluetoothDevice1Name,
                       device::GetSerialPortProfileUUID());

  // Create a filter which will select only the Bluetooth port.
  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  auto filter = blink::mojom::SerialPortFilter::New();
  filter->bluetooth_service_class_id = device::GetSerialPortProfileUUID();
  filters.push_back(std::move(filter));

  std::vector<device::BluetoothUUID> allowed_bluetooth_service_class_ids;
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters),
      std::move(allowed_bluetooth_service_class_ids), base::DoNothing());

  permissions::MockChooserControllerView view;
  controller->set_view(&view);

  {
    const std::u16string expected_name = kBluetoothDevice1Name;

    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
      run_loop.Quit();
    }));
    run_loop.Run();
    // Expect that only the Bluetooth port is shown thanks to the filter.
    ASSERT_EQ(1u, controller->NumOptions());
    ASSERT_EQ(expected_name, controller->GetOption(0));
  }

  // Removing the wired port should be a no-op since it is filtered out.
  EXPECT_CALL(view, OnOptionRemoved).Times(0);
  port_manager().RemovePort(port_1);
  base::RunLoop().RunUntilIdle();

  // Adding it back should be a no-op as well.
  EXPECT_CALL(view, OnOptionAdded).Times(0);
  AddPort("Test Port 1", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS0")),
          0x1234, 0x0001);
  base::RunLoop().RunUntilIdle();

  // Removing the Bluetooth port should trigger a change in the UI. This also
  // acts as a synchronization point to make sure that the changes above were
  // processed.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionRemoved(0))
        .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));
    port_manager().RemovePort(bluetooth_port_token);
    run_loop.Run();
  }
}

TEST_F(SerialChooserControllerTest, BluetoothPortFilteredButNotAllowed) {
  base::HistogramTester histogram_tester;

  base::UnguessableToken port_1 =
      AddPort("Test Port 1", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS0")),
              0x1234, 0x0001);
  // Create a non-standard Bluetooth port.
  base::UnguessableToken bluetooth_port_token =
      AddBluetoothPort(kBluetoothDevice1Address, kBluetoothDevice1Name,
                       kRandomBluetoothServiceClassId);

  // Create a filter which will select only the Bluetooth port.
  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  auto filter = blink::mojom::SerialPortFilter::New();
  filter->bluetooth_service_class_id = kRandomBluetoothServiceClassId;
  filters.push_back(std::move(filter));

  std::vector<device::BluetoothUUID> allowed_bluetooth_service_class_ids;
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters),
      std::move(allowed_bluetooth_service_class_ids), base::DoNothing());

  permissions::MockChooserControllerView view;
  controller->set_view(&view);

  {
    const std::u16string expected_name = kBluetoothDevice1Name;

    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
      // Expect that only the Bluetooth port is not shown as it is not allowed.
      EXPECT_EQ(0u, controller->NumOptions());
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  // Removing the wired port should be a no-op since it is filtered out.
  EXPECT_CALL(view, OnOptionRemoved).Times(0);
  port_manager().RemovePort(port_1);
  base::RunLoop().RunUntilIdle();

  // Adding it back should be a no-op as well.
  EXPECT_CALL(view, OnOptionAdded).Times(0);
  AddPort("Test Port 1", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS0")),
          0x1234, 0x0001);
  base::RunLoop().RunUntilIdle();

  // Removing the wired port should be a no-op since it is not allowed.
  EXPECT_CALL(view, OnOptionRemoved).Times(0);
  port_manager().RemovePort(bluetooth_port_token);
  base::RunLoop().RunUntilIdle();

  // Adding it back should be a no-op as well.
  EXPECT_CALL(view, OnOptionAdded).Times(0);
  AddBluetoothPort(kBluetoothDevice1Address, kBluetoothDevice1Name,
                   kRandomBluetoothServiceClassId);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SerialChooserControllerTest, DeviceNameDisambiguation) {
  base::HistogramTester histogram_tester;

  // A test Bluetooth service ID to be chosen.
  const BluetoothUUID kDevice1OtherServiceId(
      "105b5a98-d8e6-4f39-9432-49ae7529de74");

  // Create the test Bluetooth ports.
  auto port1_token =
      AddBluetoothPort(kBluetoothDevice1Address, kBluetoothDevice1Name,
                       device::GetSerialPortProfileUUID());
  auto port2_token = AddBluetoothPort(
      kBluetoothDevice1Address, kBluetoothDevice1Name, kDevice1OtherServiceId);
  auto port3_token =
      AddBluetoothPort(kBluetoothDevice2Address, kBluetoothDevice2Name,
                       device::GetSerialPortProfileUUID());

  // Create a filter that selects all Bluetooth ports.
  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  {
    auto filter = blink::mojom::SerialPortFilter::New();
    filter->bluetooth_service_class_id = device::GetSerialPortProfileUUID();
    filters.push_back(std::move(filter));

    filter = blink::mojom::SerialPortFilter::New();
    filter->bluetooth_service_class_id = kDevice1OtherServiceId;
    filters.push_back(std::move(filter));
  }

  std::vector<device::BluetoothUUID> allowed_bluetooth_service_class_ids;
  allowed_bluetooth_service_class_ids.push_back(kDevice1OtherServiceId);
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters),
      std::move(allowed_bluetooth_service_class_ids), base::DoNothing());

  permissions::MockChooserControllerView view;
  controller->set_view(&view);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
      EXPECT_EQ(3u, controller->NumOptions());
      for (size_t idx = 0; idx < controller->NumOptions(); idx++) {
        const device::mojom::SerialPortInfo& port_info =
            controller->GetPortForTest(idx);
        std::u16string expected_name;
        if (port_info.token == port1_token) {
          expected_name =
              u"Bluetooth #1 (00001101-0000-1000-8000-00805f9b34fb)";
        } else if (port_info.token == port2_token) {
          expected_name =
              u"Bluetooth #1 (105b5a98-d8e6-4f39-9432-49ae7529de74)";
        } else if (port_info.token == port3_token) {
          // Only one port for device #2, so expect device display name only.
          expected_name = u"Bluetooth #2";
        } else {
          FAIL() << "Unexpected port token: " << port_info.token;
        }
        EXPECT_EQ(expected_name, controller->GetOption(idx))
            << "incorrect name for index " << idx;
      }
      run_loop.Quit();
    }));
    run_loop.Run();
  }
}

class SerialChooserControllerTestWithBlockedPorts
    : public SerialChooserControllerTest {
 public:
  SerialChooserControllerTestWithBlockedPorts() {
    feature_list_.InitWithFeaturesAndParameters(
        {{kWebSerialBlocklist,
          {{kWebSerialBlocklistAdditions.name,
            "usb:1234:0002,bluetooth:aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa"}}}},
        {});
    // Reinitialize SerialBlocklist in case it was already initialized by
    // another test.
    SerialBlocklist::Get().ResetToDefaultValuesForTesting();
  }

  ~SerialChooserControllerTestWithBlockedPorts() override {
    // Clear the blocklist so that later tests are unaffected.
    feature_list_.Reset();
    SerialBlocklist::Get().ResetToDefaultValuesForTesting();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SerialChooserControllerTestWithBlockedPorts, Blocklist) {
  // Create two ports from the same vendor with different product IDs. The
  // second one is on the blocklist.
  base::UnguessableToken port_1 =
      AddPort("Test Port 1", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS0")),
              0x1234, 0x0001);
  base::UnguessableToken port_2 =
      AddPort("Test Port 2", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS1")),
              0x1234, 0x0002);
  const BluetoothUUID kBlockedBluetoothServiceClassId(
      "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa");
  base::UnguessableToken port_3 =
      AddBluetoothPort(kBluetoothDevice1Address, kBluetoothDevice1Name,
                       kBlockedBluetoothServiceClassId);

  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  std::vector<device::BluetoothUUID> allowed_bluetooth_service_class_ids;
  allowed_bluetooth_service_class_ids.push_back(
      kBlockedBluetoothServiceClassId);
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters),
      std::move(allowed_bluetooth_service_class_ids), base::DoNothing());

  permissions::MockChooserControllerView view;
  controller->set_view(&view);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
      // Expect that only the first port is shown thanks to the filter.
      EXPECT_EQ(1u, controller->NumOptions());
      EXPECT_EQ(u"Test Port 1 (ttyS0)", controller->GetOption(0));
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  // Removing the second port should be a no-op since it is filtered out.
  EXPECT_CALL(view, OnOptionRemoved).Times(0);
  port_manager().RemovePort(port_2);
  base::RunLoop().RunUntilIdle();

  // Removing the third port should be a no-op since it is filtered out.
  EXPECT_CALL(view, OnOptionRemoved).Times(0);
  port_manager().RemovePort(port_3);
  base::RunLoop().RunUntilIdle();

  // Adding it back should be a no-op as well.
  EXPECT_CALL(view, OnOptionAdded).Times(0);
  AddPort("Test Port 2", base::FilePath(FILE_PATH_LITERAL("/dev/ttyS1")),
          0x1234, 0x0002);
  base::RunLoop().RunUntilIdle();

  // Removing the first port should trigger a change in the UI. This also acts
  // as a synchronization point to make sure that the changes above were
  // processed.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionRemoved(0)).WillOnce(Invoke([&]() {
      run_loop.Quit();
    }));
    port_manager().RemovePort(port_1);
    run_loop.Run();
  }
}

TEST_F(SerialChooserControllerTest,
       NotWirelessSerialPortExclusiveNoBluetoothServiceClassIds) {
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::vector<blink::mojom::SerialPortFilterPtr>(),
      std::vector<device::BluetoothUUID>(), base::DoNothing());
  permissions::MockChooserControllerView view;
  controller->set_view(&view);

  base::RunLoop run_loop;
  // Check that there is no interaction with the bluetooth adapter.
  EXPECT_CALL(*adapter(), GetOsPermissionStatus).Times(0);
  EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
    EXPECT_EQ(0u, controller->NumOptions());
    run_loop.Quit();
  }));
  run_loop.Run();
}

TEST_F(SerialChooserControllerTest,
       NotWirelessSerialPortExclusiveEmptyFilters) {
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::vector<blink::mojom::SerialPortFilterPtr>(),
      std::vector<device::BluetoothUUID>({kRandomBluetoothServiceClassId}),
      base::DoNothing());
  permissions::MockChooserControllerView view;
  controller->set_view(&view);

  base::RunLoop run_loop;
  // Check that there is no interaction with the bluetooth adapter.
  EXPECT_CALL(*adapter(), GetOsPermissionStatus).Times(0);
  EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
    EXPECT_EQ(0u, controller->NumOptions());
    run_loop.Quit();
  }));
  run_loop.Run();
}

TEST_F(SerialChooserControllerTest,
       NotWirelessSerialPortExclusiveFiltersWithBluetoothAndUsb) {
  // Create filters that can match wired and wireless serial ports.
  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  auto filter = blink::mojom::SerialPortFilter::New();
  filter->bluetooth_service_class_id = kRandomBluetoothServiceClassId;
  filters.push_back(std::move(filter));
  filter = blink::mojom::SerialPortFilter::New();
  filter->has_vendor_id = true;
  filter->vendor_id = 0x1234;
  filter->has_product_id = true;
  filter->product_id = 0x0001;
  filters.push_back(std::move(filter));

  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters),
      std::vector<device::BluetoothUUID>({kRandomBluetoothServiceClassId}),
      base::DoNothing());
  permissions::MockChooserControllerView view;
  controller->set_view(&view);

  base::RunLoop run_loop;
  // Check that there is no interaction with the bluetooth adapter.
  EXPECT_CALL(*adapter(), GetOsPermissionStatus).Times(0);
  EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
    EXPECT_EQ(0u, controller->NumOptions());
    run_loop.Quit();
  }));
  run_loop.Run();
}

TEST_F(SerialChooserControllerTest, SystemBluetoothPermissionDenied) {
  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  auto filter = blink::mojom::SerialPortFilter::New();
  filter->bluetooth_service_class_id = kRandomBluetoothServiceClassId;
  filters.push_back(std::move(filter));

  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters),
      std::vector<device::BluetoothUUID>({kRandomBluetoothServiceClassId}),
      base::DoNothing());
  permissions::MockChooserControllerView view;
  controller->set_view(&view);

  base::RunLoop run_loop;
  EXPECT_CALL(*adapter(), GetOsPermissionStatus)
      .WillOnce(Return(device::BluetoothAdapter::PermissionStatus::kDenied));
  EXPECT_CALL(view, OnAdapterAuthorizationChanged(false)).WillOnce(Invoke([&] {
    EXPECT_EQ(0u, controller->NumOptions());
    run_loop.Quit();
  }));
  run_loop.Run();
}

TEST_F(SerialChooserControllerTest, SystemBluetoothPermissionUndetermined) {
  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  auto filter = blink::mojom::SerialPortFilter::New();
  filter->bluetooth_service_class_id = kRandomBluetoothServiceClassId;
  filters.push_back(std::move(filter));

  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters),
      std::vector<device::BluetoothUUID>({kRandomBluetoothServiceClassId}),
      base::DoNothing());
  permissions::MockChooserControllerView view;
  controller->set_view(&view);

  base::RunLoop run_loop;
  EXPECT_CALL(*adapter(), GetOsPermissionStatus)
      .WillOnce(
          Return(device::BluetoothAdapter::PermissionStatus::kUndetermined));
  EXPECT_CALL(view, OnAdapterAuthorizationChanged(false)).WillOnce(Invoke([&] {
    EXPECT_EQ(0u, controller->NumOptions());
    run_loop.Quit();
  }));
  run_loop.Run();
}

TEST_F(SerialChooserControllerTest, AdapterPowerOffThenPowerOn) {
  AddBluetoothPort(kBluetoothDevice1Address, kBluetoothDevice1Name,
                   kRandomBluetoothServiceClassId);

  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  auto filter = blink::mojom::SerialPortFilter::New();
  filter->bluetooth_service_class_id = kRandomBluetoothServiceClassId;
  filters.push_back(std::move(filter));

  // Start with adapter power off.
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters),
      std::vector<device::BluetoothUUID>({kRandomBluetoothServiceClassId}),
      base::DoNothing());
  permissions::MockChooserControllerView view;
  controller->set_view(&view);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*adapter(), IsPowered).WillOnce(Return(false));
    EXPECT_CALL(view, OnAdapterEnabledChanged(false)).WillOnce(Invoke([&] {
      EXPECT_EQ(0u, controller->NumOptions());
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  // Then adapter power on, expect to see ports.
  EXPECT_CALL(*adapter(), IsPowered).WillOnce(Return(true));
  EXPECT_CALL(view, OnAdapterEnabledChanged(true));
  adapter()->NotifyAdapterPoweredChanged(true);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
      ASSERT_EQ(1u, controller->NumOptions());
      EXPECT_EQ(kBluetoothDevice1Name, controller->GetOption(0));
      run_loop.Quit();
    }));
    run_loop.Run();
  }
}

TEST_F(SerialChooserControllerTest, AdapterPowerOffAfterOptionsInitialized) {
  AddBluetoothPort(kBluetoothDevice1Address, kBluetoothDevice1Name,
                   kRandomBluetoothServiceClassId);

  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  auto filter = blink::mojom::SerialPortFilter::New();
  filter->bluetooth_service_class_id = kRandomBluetoothServiceClassId;
  filters.push_back(std::move(filter));

  // Start with adapter power on.
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters),
      std::vector<device::BluetoothUUID>({kRandomBluetoothServiceClassId}),
      base::DoNothing());
  permissions::MockChooserControllerView view;
  controller->set_view(&view);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*adapter(), IsPowered).WillOnce(Return(true));
    EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
      ASSERT_EQ(1u, controller->NumOptions());
      EXPECT_EQ(kBluetoothDevice1Name, controller->GetOption(0));
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  // Then adapter power off.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnAdapterEnabledChanged(false)).WillOnce(Invoke([&] {
      run_loop.Quit();
    }));
    adapter()->NotifyAdapterPoweredChanged(false);
    run_loop.Run();
  }
}
