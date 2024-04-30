// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/hid/mock_hid_device_observer.h"
#include "chrome/browser/ui/hid/hid_chooser_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/permissions/mock_chooser_controller_view.h"
#include "content/public/browser/hid_chooser.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "services/device/public/cpp/hid/hid_switches.h"
#include "services/device/public/cpp/test/fake_hid_manager.h"
#include "services/device/public/cpp/test/hid_test_util.h"
#include "services/device/public/cpp/test/test_report_descriptors.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

using ::base::test::RunClosure;
using ::base::test::TestFuture;
using ::testing::_;

namespace {

constexpr char kDefaultTestUrl[] = "https://www.google.com/";

const char* const kTestPhysicalDeviceIds[] = {"1", "2", "3"};

constexpr uint16_t kVendorYubico = 0x1050;
constexpr uint16_t kProductYubicoGnubby = 0x0200;
constexpr uint16_t kUsageFidoU2f = 0x0001;

class HidChooserControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  HidChooserControllerTest() = default;
  HidChooserControllerTest(HidChooserControllerTest&) = delete;
  HidChooserControllerTest& operator=(HidChooserControllerTest&) = delete;
  ~HidChooserControllerTest() override = default;

  permissions::MockChooserControllerView& view() {
    return mock_chooser_controller_view_;
  }
  MockHidDeviceObserver& device_observer() { return mock_device_observer_; }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

    // Set fake HID manager for HidChooserContext.
    mojo::PendingRemote<device::mojom::HidManager> hid_manager;
    hid_manager_.Bind(hid_manager.InitWithNewPipeAndPassReceiver());
    base::RunLoop run_loop;
    auto* chooser_context = HidChooserContextFactory::GetForProfile(profile());
    chooser_context->SetHidManagerForTesting(
        std::move(hid_manager),
        base::BindLambdaForTesting(
            [&run_loop](std::vector<device::mojom::HidDeviceInfoPtr> devices) {
              run_loop.Quit();
            }));
    run_loop.Run();

    chooser_context->AddDeviceObserver(&mock_device_observer_);
  }

  void TearDown() override {
    auto* chooser_context = HidChooserContextFactory::GetForProfile(profile());
    chooser_context->RemoveDeviceObserver(&mock_device_observer_);

    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<HidChooserController> CreateHidChooserController(
      std::vector<blink::mojom::HidDeviceFilterPtr> filters,
      std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters = {},
      content::HidChooser::Callback callback = base::DoNothing()) {
    auto hid_chooser_controller = std::make_unique<HidChooserController>(
        main_rfh(), std::move(filters), std::move(exclusion_filters),
        std::move(callback));
    hid_chooser_controller->set_view(&mock_chooser_controller_view_);
    return hid_chooser_controller;
  }

  device::mojom::HidDeviceInfoPtr CreateAndAddFakeHidDevice(
      const std::string& physical_device_id,
      uint32_t vendor_id,
      uint16_t product_id,
      const std::string& product_string,
      const std::string& serial_number,
      uint16_t usage_page = device::mojom::kPageGenericDesktop,
      uint16_t usage = device::mojom::kGenericDesktopGamePad) {
    return hid_manager_.CreateAndAddDeviceWithTopLevelUsage(
        physical_device_id, vendor_id, product_id, product_string,
        serial_number, device::mojom::HidBusType::kHIDBusTypeUSB, usage_page,
        usage);
  }

  void ConnectDevice(const device::mojom::HidDeviceInfo& device) {
    hid_manager_.AddDevice(device.Clone());
  }

  void DisconnectDevice(const device::mojom::HidDeviceInfo& device) {
    hid_manager_.RemoveDevice(device.guid);
  }

  void UpdateDevice(const device::mojom::HidDeviceInfo& device) {
    hid_manager_.ChangeDevice(device.Clone());
  }

  blink::mojom::DeviceIdFilterPtr CreateVendorFilter(uint16_t vendor_id) {
    return blink::mojom::DeviceIdFilter::NewVendor(vendor_id);
  }

  blink::mojom::DeviceIdFilterPtr CreateVendorAndProductFilter(
      uint16_t vendor_id,
      uint16_t product_id) {
    return blink::mojom::DeviceIdFilter::NewVendorAndProduct(
        blink::mojom::VendorAndProduct::New(vendor_id, product_id));
  }

  blink::mojom::UsageFilterPtr CreatePageFilter(uint16_t usage_page) {
    return blink::mojom::UsageFilter::NewPage(usage_page);
  }

  blink::mojom::UsageFilterPtr CreateUsageAndPageFilter(uint16_t usage_page,
                                                        uint16_t usage) {
    return blink::mojom::UsageFilter::NewUsageAndPage(
        device::mojom::HidUsageAndPage::New(usage, usage_page));
  }

 private:
  device::FakeHidManager hid_manager_;
  permissions::MockChooserControllerView mock_chooser_controller_view_;
  MockHidDeviceObserver mock_device_observer_;
};

}  // namespace

TEST_F(HidChooserControllerTest, EmptyChooser) {
  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // Create the HidChooserController. There should be no options.
  auto hid_chooser_controller = CreateHidChooserController({});
  options_initialized_loop.Run();

  EXPECT_EQ(0u, hid_chooser_controller->NumOptions());
}

TEST_F(HidChooserControllerTest, AddBlockedFidoDevice) {
  base::RunLoop device_added_loop;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device blocked by vendor/product ID.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], kVendorYubico,
                            kProductYubicoGnubby, "gnubby", "001");
  device_added_loop.Run();

  // 2. Create the HidChooserController. The blocked device should be excluded.
  auto hid_chooser_controller = CreateHidChooserController({});
  options_initialized_loop.Run();

  EXPECT_EQ(0u, hid_chooser_controller->NumOptions());
}

TEST_F(HidChooserControllerTest, AddUnknownFidoDevice) {
  // Devices that expose a top-level collection with the FIDO usage page should
  // be blocked even if they aren't on the HID blocklist.
  base::RunLoop device_added_loop1;
  base::RunLoop device_added_loop2;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop1.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop2.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device blocked by HID usage.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "fido", "001",
                            device::mojom::kPageFido, kUsageFidoU2f);
  device_added_loop1.Run();

  // 2. Connect a second device blocked by HID usage.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[1], 2, 2, "fido", "002",
                            device::mojom::kPageFido, 0);
  device_added_loop2.Run();

  // 3. Create the HidChooserController. The blocked devices should be excluded.
  auto hid_chooser_controller = CreateHidChooserController({});
  options_initialized_loop.Run();

  EXPECT_EQ(0u, hid_chooser_controller->NumOptions());
}

// Test fixture that disables the HID blocklist. This has to be done during test
// setup, rather than the test body, to avoid modifications of the (globally
// shared) CommandLine racing against other threads using it.
class HidChooserControllerWithoutBlocklistTest
    : public HidChooserControllerTest {
 public:
  HidChooserControllerWithoutBlocklistTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableHidBlocklist);
  }
  ~HidChooserControllerWithoutBlocklistTest() override = default;
};

TEST_F(HidChooserControllerWithoutBlocklistTest,
       BlockedFidoDeviceAllowedWithFlag) {
  base::RunLoop device_added_loop;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device with the FIDO usage page. The device uses
  // vendor/product IDs that are blocked by the HID blocklist.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], kVendorYubico,
                            kProductYubicoGnubby, "gnubby", "001",
                            device::mojom::kPageFido, kUsageFidoU2f);
  device_added_loop.Run();

  // 2. Create the HidChooserController. The blocked device should be included.
  auto hid_chooser_controller = CreateHidChooserController({});
  options_initialized_loop.Run();

  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(u"gnubby", hid_chooser_controller->GetOption(0));
}

TEST_F(HidChooserControllerTest, AddNamedDevice) {
  base::RunLoop device_added_loop;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device with a non-empty product name string.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001");
  device_added_loop.Run();

  // 2. Create the HidChooserController. The option text should include the
  // product name.
  auto hid_chooser_controller = CreateHidChooserController({});
  options_initialized_loop.Run();

  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(u"a", hid_chooser_controller->GetOption(0));
}

TEST_F(HidChooserControllerTest, AddUnnamedDevice) {
  base::RunLoop device_added_loop;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device with an empty product name string.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "", "001");
  device_added_loop.Run();

  // 2. Create the HidChooserController. The option text should use an alternate
  // string.
  auto hid_chooser_controller = CreateHidChooserController({});
  options_initialized_loop.Run();

  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(u"Unknown Device (0001:0001)",
            hid_chooser_controller->GetOption(0));
}

TEST_F(HidChooserControllerTest, DeviceIdFilterVendorOnly) {
  base::RunLoop device_added_loop1;
  base::RunLoop device_added_loop2;
  base::RunLoop device_added_loop3;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop1.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop2.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop3.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001");
  device_added_loop1.Run();

  // 2. Connect a second device with the same vendor ID.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[1], 1, 2, "b", "002");
  device_added_loop2.Run();

  // 3. Connect a device with a different vendor ID.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[2], 2, 2, "c", "003");
  device_added_loop3.Run();

  // 4. Create the HidChooserController with a vendor ID filter. The third
  // device should be excluded.
  std::vector<blink::mojom::HidDeviceFilterPtr> filters;
  filters.push_back(
      blink::mojom::HidDeviceFilter::New(CreateVendorFilter(1), nullptr));
  auto hid_chooser_controller = CreateHidChooserController(std::move(filters));
  options_initialized_loop.Run();

  EXPECT_EQ(2u, hid_chooser_controller->NumOptions());

  std::set<std::u16string> options{hid_chooser_controller->GetOption(0),
                                   hid_chooser_controller->GetOption(1)};
  EXPECT_THAT(options, testing::UnorderedElementsAre(u"a", u"b"));
}

TEST_F(HidChooserControllerTest, DeviceIdFilterVendorAndProduct) {
  base::RunLoop device_added_loop1;
  base::RunLoop device_added_loop2;
  base::RunLoop device_added_loop3;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop1.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop2.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop3.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001");
  device_added_loop1.Run();

  // 2. Connect a device with matching vendor ID but non-matching product ID.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[1], 1, 2, "b", "002");
  device_added_loop2.Run();

  // 3. Connect a device with non-matching vendor and product IDs.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[2], 2, 2, "c", "003");
  device_added_loop3.Run();

  // 4. Create the HidChooserController with a vendor and product ID filter. The
  // second and third devices should be excluded.
  std::vector<blink::mojom::HidDeviceFilterPtr> filters;
  filters.push_back(blink::mojom::HidDeviceFilter::New(
      CreateVendorAndProductFilter(1, 1), nullptr));
  auto hid_chooser_controller = CreateHidChooserController(std::move(filters));
  options_initialized_loop.Run();

  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(u"a", hid_chooser_controller->GetOption(0));
}

TEST_F(HidChooserControllerTest, UsageFilterUsagePageOnly) {
  base::RunLoop device_added_loop1;
  base::RunLoop device_added_loop2;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop1.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop2.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  device_added_loop1.Run();

  // 2. Connect a device with a different top-level usage page.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[1], 2, 2, "b", "002",
                            device::mojom::kPageSimulation, 5);
  device_added_loop2.Run();

  // 3. Create the HidChooserController with a usage page filter. The second
  // device should be excluded.
  std::vector<blink::mojom::HidDeviceFilterPtr> filters;
  filters.push_back(blink::mojom::HidDeviceFilter::New(
      nullptr, CreatePageFilter(device::mojom::kPageGenericDesktop)));
  auto hid_chooser_controller = CreateHidChooserController(std::move(filters));
  options_initialized_loop.Run();

  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(u"a", hid_chooser_controller->GetOption(0));
}

TEST_F(HidChooserControllerTest, UsageFilterUsageAndPage) {
  base::RunLoop device_added_loop1;
  base::RunLoop device_added_loop2;
  base::RunLoop device_added_loop3;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop1.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop2.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop3.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  device_added_loop1.Run();

  // 2. Connect a device with matching usage page but non-matching usage.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[1], 2, 2, "b", "002",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopKeyboard);
  device_added_loop2.Run();

  // 3. Connect a device with non-matching usage page and usage.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[2], 3, 3, "c", "003",
                            device::mojom::kPageSimulation, 5);
  device_added_loop3.Run();

  // 4. Create the HidChooserController with a usage page and usage filter. The
  // second and third devices should be excluded.
  std::vector<blink::mojom::HidDeviceFilterPtr> filters;
  filters.push_back(blink::mojom::HidDeviceFilter::New(
      nullptr,
      CreateUsageAndPageFilter(device::mojom::kPageGenericDesktop,
                               device::mojom::kGenericDesktopGamePad)));
  auto hid_chooser_controller = CreateHidChooserController(std::move(filters));
  options_initialized_loop.Run();

  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(u"a", hid_chooser_controller->GetOption(0));
}

TEST_F(HidChooserControllerTest, DeviceIdAndUsageFilterIntersection) {
  base::RunLoop device_added_loop1;
  base::RunLoop device_added_loop2;
  base::RunLoop device_added_loop3;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop1.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop2.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop3.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  device_added_loop1.Run();

  // 2. Connect a device with matching usage page and usage but non-matching
  // vendor and product IDs.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[1], 2, 2, "b", "002",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  device_added_loop2.Run();

  // 3. Connect a device with matching vendor and product IDs but non-matching
  // usage page and usage.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[2], 1, 1, "c", "003",
                            device::mojom::kPageSimulation, 5);
  device_added_loop3.Run();

  // 4. Create the HidChooserController with a filter that tests vendor ID,
  // product ID, usage page, and usage. The second and third devices should be
  // excluded.
  std::vector<blink::mojom::HidDeviceFilterPtr> filters;
  filters.push_back(blink::mojom::HidDeviceFilter::New(
      CreateVendorAndProductFilter(1, 1),
      CreateUsageAndPageFilter(device::mojom::kPageGenericDesktop,
                               device::mojom::kGenericDesktopGamePad)));
  auto hid_chooser_controller = CreateHidChooserController(std::move(filters));
  options_initialized_loop.Run();

  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(u"a", hid_chooser_controller->GetOption(0));
}

TEST_F(HidChooserControllerTest, DeviceIdAndUsageFilterUnion) {
  base::RunLoop device_added_loop1;
  base::RunLoop device_added_loop2;
  base::RunLoop device_added_loop3;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop1.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop2.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop3.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  device_added_loop1.Run();

  // 2. Connect a device with matching usage page and usage but non-matching
  // vendor and product IDs.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[1], 2, 2, "b", "002",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  device_added_loop2.Run();

  // 3. Connect a device with matching vendor and product IDs but non-matching
  // usage page and usage.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[2], 1, 1, "c", "003",
                            device::mojom::kPageSimulation, 5);
  device_added_loop3.Run();

  // 4. Create the HidChooserController with a vendor/product ID filter and a
  // usage page/usage filter. No devices should be excluded.
  std::vector<blink::mojom::HidDeviceFilterPtr> filters;
  filters.push_back(blink::mojom::HidDeviceFilter::New(
      CreateVendorAndProductFilter(1, 1), nullptr));
  filters.push_back(blink::mojom::HidDeviceFilter::New(
      nullptr,
      CreateUsageAndPageFilter(device::mojom::kPageGenericDesktop,
                               device::mojom::kGenericDesktopGamePad)));
  auto hid_chooser_controller = CreateHidChooserController(std::move(filters));
  options_initialized_loop.Run();

  EXPECT_EQ(3u, hid_chooser_controller->NumOptions());
}

TEST_F(HidChooserControllerTest, ExcludeDeviceIdFilterVendorOnly) {
  base::RunLoop device_added_loop1;
  base::RunLoop device_added_loop2;
  base::RunLoop device_added_loop3;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop1.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop2.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop3.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001");
  device_added_loop1.Run();

  // 2. Connect a second device with a different vendor ID.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[1], 2, 1, "b", "002");
  device_added_loop2.Run();

  // 3. Connect a third device with the same vendor ID as second device.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[2], 2, 2, "c", "003");
  device_added_loop3.Run();

  // 4. Create the HidChooserController with a vendor ID exclusion filter. The
  // first device should be excluded.
  std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters;
  exclusion_filters.push_back(
      blink::mojom::HidDeviceFilter::New(CreateVendorFilter(1), nullptr));
  auto hid_chooser_controller =
      CreateHidChooserController({}, std::move(exclusion_filters));
  options_initialized_loop.Run();

  EXPECT_EQ(2u, hid_chooser_controller->NumOptions());

  std::set<std::u16string> options{hid_chooser_controller->GetOption(0),
                                   hid_chooser_controller->GetOption(1)};
  EXPECT_THAT(options, testing::UnorderedElementsAre(u"b", u"c"));
}

TEST_F(HidChooserControllerTest, ExcludeDeviceIdFilterVendorAndProduct) {
  base::RunLoop device_added_loop1;
  base::RunLoop device_added_loop2;
  base::RunLoop device_added_loop3;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop1.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop2.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop3.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device with non-matching product ID.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001");
  device_added_loop1.Run();

  // 2. Connect a device with matching vendor and product IDs.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[1], 1, 2, "b", "002");
  device_added_loop2.Run();

  // 3. Connect a device with non-matching vendor ID.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[2], 2, 2, "c", "003");
  device_added_loop3.Run();

  // 4. Create the HidChooserController with a vendor and product ID exclusion
  // filter. The second device should be excluded.
  std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters;
  exclusion_filters.push_back(blink::mojom::HidDeviceFilter::New(
      CreateVendorAndProductFilter(1, 2), nullptr));
  auto hid_chooser_controller =
      CreateHidChooserController({}, std::move(exclusion_filters));
  options_initialized_loop.Run();

  EXPECT_EQ(2u, hid_chooser_controller->NumOptions());

  std::set<std::u16string> options{hid_chooser_controller->GetOption(0),
                                   hid_chooser_controller->GetOption(1)};
  EXPECT_THAT(options, testing::UnorderedElementsAre(u"a", u"c"));
}

TEST_F(HidChooserControllerTest, ExcludeUsageFilterUsagePageOnly) {
  base::RunLoop device_added_loop1;
  base::RunLoop device_added_loop2;
  base::RunLoop device_added_loop3;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop1.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop2.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop3.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  device_added_loop1.Run();

  // 2. Connect a device with a different top-level usage page.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[1], 2, 2, "b", "002",
                            device::mojom::kPageSimulation,
                            device::mojom::kGenericDesktopGamePad);
  device_added_loop2.Run();

  // 3. Connect a third device with the same top-level usage page as second
  // device.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[2], 3, 3, "c", "003",
                            device::mojom::kPageSimulation,
                            device::mojom::kGenericDesktopJoystick);
  device_added_loop3.Run();

  // 4. Create the HidChooserController with a usage page exclusion filter. The
  // first device should be excluded.
  std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters;
  exclusion_filters.push_back(blink::mojom::HidDeviceFilter::New(
      nullptr, CreatePageFilter(device::mojom::kPageGenericDesktop)));
  auto hid_chooser_controller =
      CreateHidChooserController({}, std::move(exclusion_filters));
  options_initialized_loop.Run();

  EXPECT_EQ(2u, hid_chooser_controller->NumOptions());

  std::set<std::u16string> options{hid_chooser_controller->GetOption(0),
                                   hid_chooser_controller->GetOption(1)};
  EXPECT_THAT(options, testing::UnorderedElementsAre(u"b", u"c"));
}

TEST_F(HidChooserControllerTest, ExcludeUsageFilterUsageAndPage) {
  base::RunLoop device_added_loop1;
  base::RunLoop device_added_loop2;
  base::RunLoop device_added_loop3;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop1.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop2.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop3.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device with matching usage page but non-matching usage.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  device_added_loop1.Run();

  // 2. Connect a device with matching usage page and usage.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[1], 2, 2, "b", "002",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopKeyboard);
  device_added_loop2.Run();

  // 3. Connect a device with non-matching usage page and usage.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[2], 3, 3, "c", "003",
                            device::mojom::kPageSimulation, 5);
  device_added_loop3.Run();

  // 4. Create the HidChooserController with a usage page and usage filter. The
  // second device should be excluded.
  std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters;
  exclusion_filters.push_back(blink::mojom::HidDeviceFilter::New(
      nullptr,
      CreateUsageAndPageFilter(device::mojom::kPageGenericDesktop,
                               device::mojom::kGenericDesktopKeyboard)));
  auto hid_chooser_controller =
      CreateHidChooserController({}, std::move(exclusion_filters));
  options_initialized_loop.Run();

  EXPECT_EQ(2u, hid_chooser_controller->NumOptions());

  std::set<std::u16string> options{hid_chooser_controller->GetOption(0),
                                   hid_chooser_controller->GetOption(1)};
  EXPECT_THAT(options, testing::UnorderedElementsAre(u"a", u"c"));
}

TEST_F(HidChooserControllerTest, ExcludeMatchingDeviceIdFilterVendorOnly) {
  base::RunLoop device_added_loop1;
  base::RunLoop device_added_loop2;
  base::RunLoop device_added_loop3;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop1.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop2.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop3.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device with non-matching product ID.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001");
  device_added_loop1.Run();

  // 2. Connect a device with matching vendor and product IDs.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[1], 1, 2, "b", "002");
  device_added_loop2.Run();

  // 3. Connect a device with non-matching vendor and product IDs.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[2], 2, 2, "c", "003");
  device_added_loop3.Run();

  // 4. Create the HidChooserController with a vendor ID filter and a vendor and
  // product ID exclusion filter. The second and third devices should be
  // excluded.
  std::vector<blink::mojom::HidDeviceFilterPtr> filters;
  filters.push_back(
      blink::mojom::HidDeviceFilter::New(CreateVendorFilter(1), nullptr));
  std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters;
  exclusion_filters.push_back(blink::mojom::HidDeviceFilter::New(
      CreateVendorAndProductFilter(1, 2), nullptr));
  auto hid_chooser_controller = CreateHidChooserController(
      std::move(filters), std::move(exclusion_filters));
  options_initialized_loop.Run();

  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(u"a", hid_chooser_controller->GetOption(0));
}

TEST_F(HidChooserControllerTest, ExcludeMatchingUsageFilterPageOnly) {
  base::RunLoop device_added_loop1;
  base::RunLoop device_added_loop2;
  base::RunLoop device_added_loop3;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop1.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop2.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop3.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device with matching page and usage.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  device_added_loop1.Run();

  // 2. Connect a device with non-matching usage.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[1], 2, 2, "b", "002",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopJoystick);
  device_added_loop2.Run();

  // 3. Connect a device with non-matching page and usage.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[2], 3, 3, "c", "003",
                            device::mojom::kPageSimulation,
                            device::mojom::kGenericDesktopJoystick);
  device_added_loop3.Run();

  // 4. Create the HidChooserController with a page filter and a usage and page
  // exclusion filter. The second and third devices should be excluded.
  std::vector<blink::mojom::HidDeviceFilterPtr> filters;
  filters.push_back(blink::mojom::HidDeviceFilter::New(
      nullptr, CreatePageFilter(device::mojom::kPageGenericDesktop)));
  std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters;
  exclusion_filters.push_back(blink::mojom::HidDeviceFilter::New(
      nullptr,
      CreateUsageAndPageFilter(device::mojom::kPageGenericDesktop,
                               device::mojom::kGenericDesktopJoystick)));

  auto hid_chooser_controller = CreateHidChooserController(
      std::move(filters), std::move(exclusion_filters));
  options_initialized_loop.Run();

  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(u"a", hid_chooser_controller->GetOption(0));
}

TEST_F(HidChooserControllerTest, OneOptionForSamePhysicalDevice) {
  base::RunLoop device_added_loop1;
  base::RunLoop device_added_loop2;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop1.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop2.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  base::MockCallback<content::HidChooser::Callback> callback;
  base::RunLoop callback_loop;
  std::vector<device::mojom::HidDeviceInfoPtr> devices;
  EXPECT_CALL(callback, Run(_))
      .WillOnce([&devices, &callback_loop](
                    std::vector<device::mojom::HidDeviceInfoPtr> d) {
        devices = std::move(d);
        callback_loop.Quit();
      });

  // 1. Connect a device.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  device_added_loop1.Run();

  // 2. Connect a second device with the same physical device ID.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001",
                            device::mojom::kPageSimulation, 5);
  device_added_loop2.Run();

  // 3. Create the HidChooserController and register a callback to get the
  // returned device list. There should be a single chooser option representing
  // both devices.
  auto hid_chooser_controller =
      CreateHidChooserController({}, {}, callback.Get());
  options_initialized_loop.Run();

  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(u"a", hid_chooser_controller->GetOption(0));

  // 4. Select the chooser option. The returned device list should include both
  // devices.
  hid_chooser_controller->Select({0});
  callback_loop.Run();

  EXPECT_EQ(2u, devices.size());
  EXPECT_EQ(kTestPhysicalDeviceIds[0], devices[0]->physical_device_id);
  EXPECT_EQ(kTestPhysicalDeviceIds[0], devices[1]->physical_device_id);
  EXPECT_NE(devices[0]->guid, devices[1]->guid);

  // Regression test for https://crbug.com/1069057. Ensure that the
  // set of options is still valid after the callback is run.
  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(u"a", hid_chooser_controller->GetOption(0));
}

TEST_F(HidChooserControllerTest, NoMergeWithDifferentPhysicalDeviceIds) {
  base::RunLoop device_added_loop1;
  base::RunLoop device_added_loop2;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop1.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop2.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  device_added_loop1.Run();

  // 2. Connect a second device with the same info as the first device except
  // for the physical device ID.
  CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[1], 1, 1, "a", "001",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  device_added_loop2.Run();

  // 3. Create the HidChooserController. The devices should have separate
  // chooser options.
  auto hid_chooser_controller = CreateHidChooserController({});
  options_initialized_loop.Run();

  EXPECT_EQ(2u, hid_chooser_controller->NumOptions());
}

TEST_F(HidChooserControllerTest, NoMergeWithEmptyPhysicalDeviceId) {
  base::RunLoop device_added_loop1;
  base::RunLoop device_added_loop2;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop1.QuitClosure()))
      .WillOnce(RunClosure(device_added_loop2.QuitClosure()));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  // 1. Connect a device with an empty physical device ID.
  CreateAndAddFakeHidDevice("", 1, 1, "a", "001",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  device_added_loop1.Run();

  // 2. Connect a second device with an empty physical device ID.
  CreateAndAddFakeHidDevice("", 1, 1, "a", "001",
                            device::mojom::kPageSimulation, 5);
  device_added_loop2.Run();

  // 3. Create the HidChooserController. The devices should have separate
  // chooser options.
  auto hid_chooser_controller = CreateHidChooserController({});
  options_initialized_loop.Run();

  EXPECT_EQ(2u, hid_chooser_controller->NumOptions());
}

TEST_F(HidChooserControllerTest, DeviceConnectAddsOption) {
  EXPECT_CALL(device_observer(), OnDeviceAdded(_));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  base::RunLoop option_added_loop;
  EXPECT_CALL(view(), OnOptionAdded(0))
      .WillOnce(RunClosure(option_added_loop.QuitClosure()));

  // 1. Create the HidChooserController.
  auto hid_chooser_controller = CreateHidChooserController({});
  options_initialized_loop.Run();

  EXPECT_EQ(0u, hid_chooser_controller->NumOptions());

  // 2. Connect a device and verify a chooser option is added.
  auto device =
      CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001",
                                device::mojom::kPageGenericDesktop,
                                device::mojom::kGenericDesktopGamePad);
  option_added_loop.Run();

  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
}

TEST_F(HidChooserControllerTest, DeviceDisconnectRemovesOption) {
  base::RunLoop device_added_loop;
  EXPECT_CALL(device_observer(), OnDeviceAdded(_))
      .WillOnce(RunClosure(device_added_loop.QuitClosure()));

  EXPECT_CALL(device_observer(), OnDeviceRemoved(_));

  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized())
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));

  base::RunLoop option_removed_loop;
  EXPECT_CALL(view(), OnOptionRemoved(0))
      .WillOnce(RunClosure(option_removed_loop.QuitClosure()));

  // 1. Connect a device.
  auto device =
      CreateAndAddFakeHidDevice(kTestPhysicalDeviceIds[0], 1, 1, "a", "001",
                                device::mojom::kPageGenericDesktop,
                                device::mojom::kGenericDesktopGamePad);
  device_added_loop.Run();

  // 2. Create the HidChooserController and verify that the device is included.
  auto hid_chooser_controller = CreateHidChooserController({});
  options_initialized_loop.Run();

  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());

  // 3. Disconnect the device and verify the chooser option is removed.
  DisconnectDevice(*device);
  option_removed_loop.Run();

  EXPECT_EQ(0u, hid_chooser_controller->NumOptions());
}

namespace {

device::mojom::HidDeviceInfoPtr CreateDeviceWithOneCollection(
    const std::string& guid) {
  auto device_info = device::mojom::HidDeviceInfo::New();
  device_info->guid = guid;
  device_info->physical_device_id = "physical-device-id";
  device_info->vendor_id = 1;
  device_info->product_id = 1;
  device_info->product_name = "a";
  auto collection = device::mojom::HidCollectionInfo::New();
  collection->usage = device::mojom::HidUsageAndPage::New(1, 1);
  collection->input_reports.push_back(
      device::mojom::HidReportDescription::New());
  device_info->collections.push_back(std::move(collection));
  return device_info;
}

device::mojom::HidDeviceInfoPtr CreateDeviceWithTwoCollections(
    const std::string guid) {
  auto device_info = CreateDeviceWithOneCollection(guid);
  auto collection = device::mojom::HidCollectionInfo::New();
  collection->usage = device::mojom::HidUsageAndPage::New(2, 2);
  collection->output_reports.push_back(
      device::mojom::HidReportDescription::New());
  device_info->collections.push_back(std::move(collection));
  return device_info;
}

}  // namespace

TEST_F(HidChooserControllerTest, DeviceChangeUpdatesDeviceInfo) {
  const char kTestGuid[] = "guid";

  // Connect a partially-initialized device with one collection.
  base::RunLoop device_added_loop;
  EXPECT_CALL(device_observer(), OnDeviceAdded).WillOnce([&](const auto& d) {
    EXPECT_EQ(d.guid, kTestGuid);
    EXPECT_EQ(d.collections.size(), 1u);
    device_added_loop.Quit();
  });
  auto partial_device = CreateDeviceWithOneCollection(kTestGuid);
  ConnectDevice(*partial_device);
  device_added_loop.Run();

  // Create the HidChooserController.
  base::MockCallback<content::HidChooser::Callback> callback;
  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized)
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));
  auto hid_chooser_controller =
      CreateHidChooserController({}, {}, callback.Get());
  options_initialized_loop.Run();

  // Check that the option is present.
  EXPECT_EQ(hid_chooser_controller->NumOptions(), 1u);
  EXPECT_EQ(u"a", hid_chooser_controller->GetOption(0));

  // Update the device to add another collection.
  base::RunLoop device_changed_loop;
  EXPECT_CALL(device_observer(), OnDeviceChanged).WillOnce([&](const auto& d) {
    EXPECT_EQ(d.guid, kTestGuid);
    EXPECT_EQ(d.collections.size(), 2u);
    device_changed_loop.Quit();
  });
  auto complete_device = CreateDeviceWithTwoCollections(kTestGuid);
  UpdateDevice(*complete_device);
  device_changed_loop.Run();

  // Check that the option is still present and the name has not changed.
  EXPECT_EQ(hid_chooser_controller->NumOptions(), 1u);
  EXPECT_EQ(u"a", hid_chooser_controller->GetOption(0));

  // Select the option and check that only the updated device info is returned.
  base::RunLoop callback_loop;
  EXPECT_CALL(callback, Run).WillOnce([&](auto devices) {
    ASSERT_EQ(devices.size(), 1u);
    EXPECT_EQ(devices[0]->guid, kTestGuid);
    EXPECT_EQ(devices[0]->collections.size(), 2u);
    callback_loop.Quit();
  });
  hid_chooser_controller->Select({0});
  callback_loop.Run();
}

TEST_F(HidChooserControllerTest, ExcludedDeviceChangedToIncludedDevice) {
  const char kTestGuid[] = "guid";

  // Connect a partially-initialized device with one collection.
  base::RunLoop device_added_loop;
  EXPECT_CALL(device_observer(), OnDeviceAdded).WillOnce([&](const auto& d) {
    EXPECT_EQ(d.guid, kTestGuid);
    EXPECT_EQ(d.collections.size(), 1u);
    device_added_loop.Quit();
  });
  auto partial_device = CreateDeviceWithOneCollection(kTestGuid);
  ConnectDevice(*partial_device);
  device_added_loop.Run();

  // Create the HidChooserController. Set a usage page filter that excludes
  // |partial_device|.
  std::vector<blink::mojom::HidDeviceFilterPtr> filters;
  filters.push_back(
      blink::mojom::HidDeviceFilter::New(nullptr, CreatePageFilter(2)));
  base::MockCallback<content::HidChooser::Callback> callback;
  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized)
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));
  auto hid_chooser_controller =
      CreateHidChooserController(std::move(filters), {}, callback.Get());
  options_initialized_loop.Run();

  // Check that the option is not present.
  EXPECT_EQ(hid_chooser_controller->NumOptions(), 0u);

  // Update the device to add another collection. Now the device should be
  // allowed by the filter.
  base::RunLoop option_added_loop;
  EXPECT_CALL(view(), OnOptionAdded(0))
      .WillOnce(RunClosure(option_added_loop.QuitClosure()));
  EXPECT_CALL(device_observer(), OnDeviceChanged).WillOnce([&](const auto& d) {
    EXPECT_EQ(d.guid, kTestGuid);
    EXPECT_EQ(d.collections.size(), 2u);
  });
  auto complete_device = CreateDeviceWithTwoCollections(kTestGuid);
  UpdateDevice(*complete_device);
  option_added_loop.Run();

  // Check that the option has been added.
  EXPECT_EQ(hid_chooser_controller->NumOptions(), 1u);
  EXPECT_EQ(u"a", hid_chooser_controller->GetOption(0));

  // Select the option and check that only the updated device info is returned.
  base::RunLoop callback_loop;
  EXPECT_CALL(callback, Run).WillOnce([&](auto devices) {
    ASSERT_EQ(devices.size(), 1u);
    EXPECT_EQ(devices[0]->guid, kTestGuid);
    EXPECT_EQ(devices[0]->collections.size(), 2u);
    callback_loop.Quit();
  });
  hid_chooser_controller->Select({0});
  callback_loop.Run();
}

TEST_F(HidChooserControllerTest, DeviceChangedToBlockedDevice) {
  const char kTestGuid[] = "guid";

  // Connect a partially-initialized device with one collection.
  base::RunLoop device_added_loop;
  EXPECT_CALL(device_observer(), OnDeviceAdded).WillOnce([&](const auto& d) {
    EXPECT_EQ(d.guid, kTestGuid);
    EXPECT_EQ(d.collections.size(), 1u);
    device_added_loop.Quit();
  });
  auto partial_device = CreateDeviceWithOneCollection(kTestGuid);
  ConnectDevice(*partial_device);
  device_added_loop.Run();

  // Create the HidChooserController.
  base::RunLoop options_initialized_loop;
  EXPECT_CALL(view(), OnOptionsInitialized)
      .WillOnce(RunClosure(options_initialized_loop.QuitClosure()));
  auto hid_chooser_controller = CreateHidChooserController({});
  options_initialized_loop.Run();

  // Check that the option is present.
  EXPECT_EQ(hid_chooser_controller->NumOptions(), 1u);
  EXPECT_EQ(u"a", hid_chooser_controller->GetOption(0));

  // Update the device to add another collection with the FIDO usage page.
  // Devices with any FIDO collection are blocked, so the chooser option should
  // be removed.
  base::RunLoop option_removed_loop;
  EXPECT_CALL(view(), OnOptionRemoved(0))
      .WillOnce(RunClosure(option_removed_loop.QuitClosure()));
  EXPECT_CALL(device_observer(), OnDeviceChanged).WillOnce([&](const auto& d) {
    EXPECT_EQ(d.guid, kTestGuid);
    EXPECT_EQ(d.collections.size(), 2u);
  });
  auto complete_device = CreateDeviceWithTwoCollections(kTestGuid);
  complete_device->collections[1]->usage->usage_page = device::mojom::kPageFido;
  UpdateDevice(*complete_device);
  option_removed_loop.Run();

  // Check that the option has been removed.
  EXPECT_EQ(hid_chooser_controller->NumOptions(), 0u);
}

class HidChooserControllerFidoTest : public HidChooserControllerTest,
                                     public testing::WithParamInterface<bool> {
};

TEST_P(HidChooserControllerFidoTest, FidoDeviceShownWithPrivilegedOrigin) {
  const bool is_fido_allowed = GetParam();
  if (is_fido_allowed) {
    // Use the gnubbyd-v3 dev key for testing since it is already on the
    // allowlist.
    OverrideLastCommittedOrigin(
        main_rfh(),
        url::Origin::Create(
            GURL("chrome-extension://ckcendljdlmgnhghiaomidhiiclmapok/")));
  }

  // Connect a FIDO device.
  TestFuture<device::mojom::HidDeviceInfoPtr> connected_device;
  EXPECT_CALL(device_observer(), OnDeviceAdded).WillOnce([&](const auto& d) {
    connected_device.SetValue(d.Clone());
  });
  auto device = device::CreateDeviceFromReportDescriptor(
      /*vendor_id=*/0x1234,
      /*product_id=*/0xabcd, device::TestReportDescriptors::FidoU2fHid());
  ConnectDevice(*device);
  ASSERT_EQ(1u, connected_device.Get()->collections.size());
  ASSERT_TRUE(connected_device.Get()->collections[0]->usage);
  EXPECT_EQ(device::mojom::kPageFido,
            connected_device.Get()->collections[0]->usage->usage_page);

  // Create the HidChooserController.
  TestFuture<bool> options_initialized;
  EXPECT_CALL(view(), OnOptionsInitialized).WillOnce([&]() {
    options_initialized.SetValue(true);
  });
  TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> selected_devices;
  auto hid_chooser_controller =
      CreateHidChooserController({}, {}, selected_devices.GetCallback());
  EXPECT_TRUE(options_initialized.Get());

  // If the origin is not allowed to access FIDO reports there should be no
  // devices in the chooser.
  if (!is_fido_allowed) {
    EXPECT_EQ(0u, hid_chooser_controller->NumOptions());
    return;
  }

  // If the origin is allowed to access FIDO reports there should be one option.
  ASSERT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(u"Test Device", hid_chooser_controller->GetOption(0));

  // Select the option and check that the returned device contains FIDO reports.
  hid_chooser_controller->Select({0});
  ASSERT_EQ(1u, selected_devices.Get().size());
  EXPECT_EQ(device->guid, selected_devices.Get()[0]->guid);
  ASSERT_EQ(1u, selected_devices.Get()[0]->collections.size());
  const auto& collection = selected_devices.Get()[0]->collections[0];
  ASSERT_TRUE(collection->usage);
  EXPECT_EQ(device::mojom::kPageFido, collection->usage->usage_page);
  EXPECT_EQ(1u, collection->input_reports.size());
  EXPECT_EQ(1u, collection->output_reports.size());
}

INSTANTIATE_TEST_SUITE_P(HidChooserControllerFidoTests,
                         HidChooserControllerFidoTest,
                         testing::Values(false, true));
