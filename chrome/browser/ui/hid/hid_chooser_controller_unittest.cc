// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/ui/hid/hid_chooser_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/hid_chooser.h"
#include "content/public/test/web_contents_tester.h"
#include "services/device/public/cpp/hid/fake_hid_manager.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace {

const char kDefaultTestUrl[] = "https://www.google.com/";

const uint16_t kYubicoVendorId = 0x1050;
const uint16_t kYubicoGnubbyProductId = 0x0200;

class FakeHidChooserView : public ChooserController::View {
 public:
  FakeHidChooserView() {}

  void set_options_initialized_quit_closure(base::OnceClosure quit_closure) {
    options_initialized_quit_closure_ = std::move(quit_closure);
  }

  // ChooserController::View:
  void OnOptionAdded(size_t index) override {}
  void OnOptionRemoved(size_t index) override {}
  void OnOptionsInitialized() override {
    if (options_initialized_quit_closure_)
      std::move(options_initialized_quit_closure_).Run();
  }
  void OnOptionUpdated(size_t index) override {}
  void OnAdapterEnabledChanged(bool enabled) override {}
  void OnRefreshStateChanged(bool enabled) override {}

 private:
  base::OnceClosure options_initialized_quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(FakeHidChooserView);
};

}  // namespace

class HidChooserControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  HidChooserControllerTest() {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

    // Set fake HID manager for HidChooserContext.
    mojo::PendingRemote<device::mojom::HidManager> hid_manager;
    hid_manager_.Bind(hid_manager.InitWithNewPipeAndPassReceiver());
    HidChooserContextFactory::GetForProfile(profile())->SetHidManagerForTesting(
        std::move(hid_manager));
  }

  std::unique_ptr<HidChooserController> CreateHidChooserController(
      std::vector<blink::mojom::HidDeviceFilterPtr>& filters) {
    auto hid_chooser_controller = std::make_unique<HidChooserController>(
        main_rfh(), std::move(filters), content::HidChooser::Callback());
    hid_chooser_controller->set_view(&fake_hid_chooser_view_);
    return hid_chooser_controller;
  }

  std::unique_ptr<HidChooserController>
  CreateHidChooserControllerWithoutFilters() {
    std::vector<blink::mojom::HidDeviceFilterPtr> filters;
    return CreateHidChooserController(filters);
  }

  device::mojom::HidDeviceInfoPtr CreateAndAddFakeHidDevice(
      uint32_t vendor_id,
      uint16_t product_id,
      const std::string& product_string,
      const std::string& serial_number,
      uint16_t usage_page = device::mojom::kPageGenericDesktop,
      uint16_t usage = device::mojom::kGenericDesktopGamePad) {
    return hid_manager_.CreateAndAddDeviceWithTopLevelUsage(
        vendor_id, product_id, product_string, serial_number,
        device::mojom::HidBusType::kHIDBusTypeUSB, usage_page, usage);
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

 protected:
  device::FakeHidManager hid_manager_;
  FakeHidChooserView fake_hid_chooser_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HidChooserControllerTest);
};

TEST_F(HidChooserControllerTest, EmptyChooser) {
  auto hid_chooser_controller = CreateHidChooserControllerWithoutFilters();
  base::RunLoop run_loop;
  fake_hid_chooser_view_.set_options_initialized_quit_closure(
      run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(0u, hid_chooser_controller->NumOptions());
}

TEST_F(HidChooserControllerTest, AddBlockedFidoDevice) {
  // FIDO U2F devices (and other devices on the USB blocklist) should be
  // excluded from the device chooser.
  auto hid_chooser_controller = CreateHidChooserControllerWithoutFilters();
  base::RunLoop run_loop;
  fake_hid_chooser_view_.set_options_initialized_quit_closure(
      run_loop.QuitClosure());
  CreateAndAddFakeHidDevice(kYubicoVendorId, kYubicoGnubbyProductId, "gnubby",
                            "001");
  run_loop.Run();
  EXPECT_EQ(0u, hid_chooser_controller->NumOptions());
}

TEST_F(HidChooserControllerTest, AddUnknownFidoDevice) {
  // Devices that expose a top-level collection with the FIDO usage page should
  // be blocked even if they aren't on the USB blocklist.
  const uint16_t kFidoU2fHidUsage = 1;
  auto hid_chooser_controller = CreateHidChooserControllerWithoutFilters();
  base::RunLoop run_loop;
  fake_hid_chooser_view_.set_options_initialized_quit_closure(
      run_loop.QuitClosure());
  CreateAndAddFakeHidDevice(1, 1, "fido", "001", device::mojom::kPageFido,
                            kFidoU2fHidUsage);
  CreateAndAddFakeHidDevice(2, 2, "fido", "002", device::mojom::kPageFido, 0);
  run_loop.Run();
  EXPECT_EQ(0u, hid_chooser_controller->NumOptions());
}

TEST_F(HidChooserControllerTest, AddNamedDevice) {
  auto hid_chooser_controller = CreateHidChooserControllerWithoutFilters();
  base::RunLoop run_loop;
  fake_hid_chooser_view_.set_options_initialized_quit_closure(
      run_loop.QuitClosure());
  CreateAndAddFakeHidDevice(1, 1, "a", "001");
  run_loop.Run();
  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("a (Vendor: 0x0001, Product: 0x0001)"),
            hid_chooser_controller->GetOption(0));
}

TEST_F(HidChooserControllerTest, AddUnnamedDevice) {
  auto hid_chooser_controller = CreateHidChooserControllerWithoutFilters();
  base::RunLoop run_loop;
  fake_hid_chooser_view_.set_options_initialized_quit_closure(
      run_loop.QuitClosure());
  CreateAndAddFakeHidDevice(1, 1, "", "001");
  run_loop.Run();
  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(
      base::ASCIIToUTF16("Unknown Device (Vendor: 0x0001, Product: 0x0001)"),
      hid_chooser_controller->GetOption(0));
}

TEST_F(HidChooserControllerTest, DeviceIdFilterVendorOnly) {
  std::vector<blink::mojom::HidDeviceFilterPtr> filters;
  filters.push_back(
      blink::mojom::HidDeviceFilter::New(CreateVendorFilter(1), nullptr));
  auto hid_chooser_controller = CreateHidChooserController(filters);

  base::RunLoop run_loop;
  fake_hid_chooser_view_.set_options_initialized_quit_closure(
      run_loop.QuitClosure());
  CreateAndAddFakeHidDevice(1, 1, "a", "001");
  CreateAndAddFakeHidDevice(1, 2, "b", "002");
  CreateAndAddFakeHidDevice(2, 2, "c", "003");
  run_loop.Run();

  EXPECT_EQ(2u, hid_chooser_controller->NumOptions());

  std::set<base::string16> options;
  options.insert(hid_chooser_controller->GetOption(0));
  options.insert(hid_chooser_controller->GetOption(1));
  EXPECT_EQ(1u, options.count(
                    base::ASCIIToUTF16("a (Vendor: 0x0001, Product: 0x0001)")));
  EXPECT_EQ(1u, options.count(
                    base::ASCIIToUTF16("b (Vendor: 0x0001, Product: 0x0002)")));
}

TEST_F(HidChooserControllerTest, DeviceIdFilterVendorAndProduct) {
  std::vector<blink::mojom::HidDeviceFilterPtr> filters;
  filters.push_back(blink::mojom::HidDeviceFilter::New(
      CreateVendorAndProductFilter(1, 1), nullptr));
  auto hid_chooser_controller = CreateHidChooserController(filters);

  base::RunLoop run_loop;
  fake_hid_chooser_view_.set_options_initialized_quit_closure(
      run_loop.QuitClosure());
  CreateAndAddFakeHidDevice(1, 1, "a", "001");
  CreateAndAddFakeHidDevice(1, 2, "b", "002");
  CreateAndAddFakeHidDevice(2, 2, "c", "003");
  run_loop.Run();

  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("a (Vendor: 0x0001, Product: 0x0001)"),
            hid_chooser_controller->GetOption(0));
}

TEST_F(HidChooserControllerTest, UsageFilterUsagePageOnly) {
  std::vector<blink::mojom::HidDeviceFilterPtr> filters;
  filters.push_back(blink::mojom::HidDeviceFilter::New(
      nullptr, CreatePageFilter(device::mojom::kPageGenericDesktop)));
  auto hid_chooser_controller = CreateHidChooserController(filters);

  base::RunLoop run_loop;
  fake_hid_chooser_view_.set_options_initialized_quit_closure(
      run_loop.QuitClosure());
  CreateAndAddFakeHidDevice(1, 1, "a", "001",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  CreateAndAddFakeHidDevice(2, 2, "b", "002", device::mojom::kPageSimulation,
                            5);
  run_loop.Run();

  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("a (Vendor: 0x0001, Product: 0x0001)"),
            hid_chooser_controller->GetOption(0));
}

TEST_F(HidChooserControllerTest, UsageFilterUsageAndPage) {
  std::vector<blink::mojom::HidDeviceFilterPtr> filters;
  filters.push_back(blink::mojom::HidDeviceFilter::New(
      nullptr,
      CreateUsageAndPageFilter(device::mojom::kPageGenericDesktop,
                               device::mojom::kGenericDesktopGamePad)));
  auto hid_chooser_controller = CreateHidChooserController(filters);

  base::RunLoop run_loop;
  fake_hid_chooser_view_.set_options_initialized_quit_closure(
      run_loop.QuitClosure());
  CreateAndAddFakeHidDevice(1, 1, "a", "001",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  CreateAndAddFakeHidDevice(2, 2, "b", "002",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopKeyboard);
  CreateAndAddFakeHidDevice(3, 3, "c", "003", device::mojom::kPageSimulation,
                            5);
  run_loop.Run();

  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("a (Vendor: 0x0001, Product: 0x0001)"),
            hid_chooser_controller->GetOption(0));
}

TEST_F(HidChooserControllerTest, DeviceIdAndUsageFilterIntersection) {
  std::vector<blink::mojom::HidDeviceFilterPtr> filters;
  filters.push_back(blink::mojom::HidDeviceFilter::New(
      CreateVendorAndProductFilter(1, 1),
      CreateUsageAndPageFilter(device::mojom::kPageGenericDesktop,
                               device::mojom::kGenericDesktopGamePad)));
  auto hid_chooser_controller = CreateHidChooserController(filters);

  base::RunLoop run_loop;
  fake_hid_chooser_view_.set_options_initialized_quit_closure(
      run_loop.QuitClosure());
  CreateAndAddFakeHidDevice(1, 1, "a", "001",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  CreateAndAddFakeHidDevice(2, 2, "b", "002",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  CreateAndAddFakeHidDevice(1, 1, "c", "003", device::mojom::kPageSimulation,
                            5);
  run_loop.Run();

  EXPECT_EQ(1u, hid_chooser_controller->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("a (Vendor: 0x0001, Product: 0x0001)"),
            hid_chooser_controller->GetOption(0));
}

TEST_F(HidChooserControllerTest, DeviceIdAndUsageFilterUnion) {
  std::vector<blink::mojom::HidDeviceFilterPtr> filters;
  filters.push_back(blink::mojom::HidDeviceFilter::New(
      CreateVendorAndProductFilter(1, 1), nullptr));
  filters.push_back(blink::mojom::HidDeviceFilter::New(
      nullptr,
      CreateUsageAndPageFilter(device::mojom::kPageGenericDesktop,
                               device::mojom::kGenericDesktopGamePad)));
  auto hid_chooser_controller = CreateHidChooserController(filters);

  base::RunLoop run_loop;
  fake_hid_chooser_view_.set_options_initialized_quit_closure(
      run_loop.QuitClosure());
  CreateAndAddFakeHidDevice(1, 1, "a", "001",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  CreateAndAddFakeHidDevice(2, 2, "b", "002",
                            device::mojom::kPageGenericDesktop,
                            device::mojom::kGenericDesktopGamePad);
  CreateAndAddFakeHidDevice(1, 1, "c", "003", device::mojom::kPageSimulation,
                            5);
  run_loop.Run();

  EXPECT_EQ(3u, hid_chooser_controller->NumOptions());
}
