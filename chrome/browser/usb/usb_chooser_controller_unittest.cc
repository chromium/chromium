// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_chooser_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/permissions/mock_chooser_controller_view.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom-blink.h"
#include "url/gurl.h"

using testing::NiceMock;

namespace {
const char kDefaultTestUrl[] = "https://www.google.com/";
}  //  namespace

class UsbChooserControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  UsbChooserControllerTest() = default;
  UsbChooserControllerTest(const UsbChooserControllerTest&) = delete;
  UsbChooserControllerTest& operator=(const UsbChooserControllerTest&) = delete;

  permissions::MockChooserControllerView& view() {
    return mock_usb_chooser_view_;
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

    // Set fake device manager for UsbChooserContext.
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
    device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());
    UsbChooserContextFactory::GetForProfile(profile())
        ->SetDeviceManagerForTesting(std::move(device_manager));

    // Make sure the device::mojom::UsbDeviceManager::SetClient() call has
    // been received.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  std::unique_ptr<UsbChooserController> CreateUsbChooserController(
      blink::mojom::WebUsbRequestDeviceOptionsPtr options =
          blink::mojom::WebUsbRequestDeviceOptions::New(),
      blink::mojom::WebUsbService::GetPermissionCallback callback =
          base::DoNothing()) {
    auto usb_chooser_controller = std::make_unique<UsbChooserController>(
        main_rfh(), std::move(options), std::move(callback));
    usb_chooser_controller->set_view(&mock_usb_chooser_view_);
    return usb_chooser_controller;
  }

  device::mojom::UsbDeviceInfoPtr CreateAndAddFakeUsbDevice(
      const std::string& product_string,
      const std::string& serial_number) {
    return device_manager_.CreateAndAddDevice(0, 1, "Google", product_string,
                                              serial_number);
  }

  device::mojom::UsbDeviceInfoPtr CreateAndAddFakeUsbDevice(
      uint16_t vendor_id,
      uint16_t product_id) {
    return device_manager_.CreateAndAddDevice(vendor_id, product_id);
  }

  device::FakeUsbDeviceManager device_manager_;
  permissions::MockChooserControllerView mock_usb_chooser_view_;
};

TEST_F(UsbChooserControllerTest, AddDevice) {
  auto usb_chooser_controller = CreateUsbChooserController();
  EXPECT_CALL(view(), OnOptionAdded(0)).Times(1);
  CreateAndAddFakeUsbDevice("a", "001");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, usb_chooser_controller->NumOptions());
  EXPECT_EQ(u"a", usb_chooser_controller->GetOption(0));

  EXPECT_CALL(view(), OnOptionAdded(1)).Times(1);
  CreateAndAddFakeUsbDevice("b", "002");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, usb_chooser_controller->NumOptions());
  EXPECT_EQ(u"b", usb_chooser_controller->GetOption(1));

  EXPECT_CALL(view(), OnOptionAdded(2)).Times(1);
  CreateAndAddFakeUsbDevice("c", "003");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, usb_chooser_controller->NumOptions());
  EXPECT_EQ(u"c", usb_chooser_controller->GetOption(2));
}

TEST_F(UsbChooserControllerTest, RemoveDevice) {
  auto usb_chooser_controller = CreateUsbChooserController();
  auto device_a = CreateAndAddFakeUsbDevice("a", "001");
  auto device_b = CreateAndAddFakeUsbDevice("b", "002");
  auto device_c = CreateAndAddFakeUsbDevice("c", "003");
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(view(), OnOptionRemoved(1)).Times(1);
  device_manager_.RemoveDevice(device_b->guid);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, usb_chooser_controller->NumOptions());
  EXPECT_EQ(u"a", usb_chooser_controller->GetOption(0));
  EXPECT_EQ(u"c", usb_chooser_controller->GetOption(1));

  EXPECT_CALL(view(), OnOptionRemoved(0)).Times(1);
  device_manager_.RemoveDevice(device_a->guid);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, usb_chooser_controller->NumOptions());
  EXPECT_EQ(u"c", usb_chooser_controller->GetOption(0));

  EXPECT_CALL(view(), OnOptionRemoved(0)).Times(1);
  device_manager_.RemoveDevice(device_c->guid);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, usb_chooser_controller->NumOptions());
}

TEST_F(UsbChooserControllerTest, AddAndRemoveDeviceWithSameName) {
  auto usb_chooser_controller = CreateUsbChooserController();
  auto device_a_1 = CreateAndAddFakeUsbDevice("a", "001");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(u"a", usb_chooser_controller->GetOption(0));

  auto device_b = CreateAndAddFakeUsbDevice("b", "002");
  auto device_a_2 = CreateAndAddFakeUsbDevice("a", "002");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(u"a (001)", usb_chooser_controller->GetOption(0));
  EXPECT_EQ(u"b", usb_chooser_controller->GetOption(1));
  EXPECT_EQ(u"a (002)", usb_chooser_controller->GetOption(2));

  device_manager_.RemoveDevice(device_a_1->guid);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(u"b", usb_chooser_controller->GetOption(0));
  EXPECT_EQ(u"a", usb_chooser_controller->GetOption(1));
}

TEST_F(UsbChooserControllerTest, UnknownDeviceName) {
  auto usb_chooser_controller = CreateUsbChooserController();
  CreateAndAddFakeUsbDevice(123, 456);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(u"Unknown device [007b:01c8]",
            usb_chooser_controller->GetOption(0));
}

TEST_F(UsbChooserControllerTest, FilterMatchingDeviceVendorId) {
  auto filter = device::mojom::UsbDeviceFilter::New();
  filter->has_vendor_id = true;
  filter->vendor_id = 100;
  auto options = blink::mojom::WebUsbRequestDeviceOptions::New();
  options->filters.push_back(std::move(filter));
  auto usb_chooser_controller = CreateUsbChooserController(std::move(options));

  CreateAndAddFakeUsbDevice(100, 100);
  CreateAndAddFakeUsbDevice(200, 200);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, usb_chooser_controller->NumOptions());
  EXPECT_EQ(u"Unknown device [0064:0064]",
            usb_chooser_controller->GetOption(0));
}

TEST_F(UsbChooserControllerTest, FilterMatchingDeviceProductId) {
  auto filter = device::mojom::UsbDeviceFilter::New();
  filter->has_vendor_id = true;
  filter->vendor_id = 100;
  filter->has_product_id = true;
  filter->product_id = 100;
  auto options = blink::mojom::WebUsbRequestDeviceOptions::New();
  options->filters.push_back(std::move(filter));
  auto usb_chooser_controller = CreateUsbChooserController(std::move(options));

  CreateAndAddFakeUsbDevice(100, 100);
  CreateAndAddFakeUsbDevice(100, 101);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, usb_chooser_controller->NumOptions());
  EXPECT_EQ(u"Unknown device [0064:0064]",
            usb_chooser_controller->GetOption(0));
}

TEST_F(UsbChooserControllerTest, FilterExcludeMatchingDeviceVendorId) {
  auto exclusion_filter = device::mojom::UsbDeviceFilter::New();
  exclusion_filter->has_vendor_id = true;
  exclusion_filter->vendor_id = 100;
  auto options = blink::mojom::WebUsbRequestDeviceOptions::New();
  options->exclusion_filters.push_back(std::move(exclusion_filter));
  auto usb_chooser_controller = CreateUsbChooserController(std::move(options));

  CreateAndAddFakeUsbDevice(100, 100);
  CreateAndAddFakeUsbDevice(200, 200);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, usb_chooser_controller->NumOptions());
  EXPECT_EQ(u"Unknown device [00c8:00c8]",
            usb_chooser_controller->GetOption(0));
}

TEST_F(UsbChooserControllerTest, FilterExcludeMatchingDeviceProductId) {
  auto exclusion_filter = device::mojom::UsbDeviceFilter::New();
  exclusion_filter->has_vendor_id = true;
  exclusion_filter->vendor_id = 100;
  exclusion_filter->has_product_id = true;
  exclusion_filter->product_id = 100;
  auto options = blink::mojom::WebUsbRequestDeviceOptions::New();
  options->exclusion_filters.push_back(std::move(exclusion_filter));
  auto usb_chooser_controller = CreateUsbChooserController(std::move(options));

  CreateAndAddFakeUsbDevice(100, 100);
  CreateAndAddFakeUsbDevice(100, 101);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, usb_chooser_controller->NumOptions());
  EXPECT_EQ(u"Unknown device [0064:0065]",
            usb_chooser_controller->GetOption(0));
}
