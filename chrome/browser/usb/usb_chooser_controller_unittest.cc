// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_chooser_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kDefaultTestUrl[] = "https://www.google.com/";

class MockUsbChooserView : public ChooserController::View {
 public:
  MockUsbChooserView() {}

  // ChooserController::View:
  MOCK_METHOD1(OnOptionAdded, void(size_t index));
  MOCK_METHOD1(OnOptionRemoved, void(size_t index));
  void OnOptionsInitialized() override {}
  void OnOptionUpdated(size_t index) override {}
  void OnAdapterEnabledChanged(bool enabled) override {}
  void OnRefreshStateChanged(bool enabled) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockUsbChooserView);
};

}  //  namespace

class UsbChooserControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  UsbChooserControllerTest() {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    std::vector<device::mojom::UsbDeviceFilterPtr> device_filters;
    blink::mojom::WebUsbService::GetPermissionCallback callback;
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(web_contents());
    web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

    // Set fake device manager for UsbChooserContext.
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
    device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());
    UsbChooserContextFactory::GetForProfile(profile())
        ->SetDeviceManagerForTesting(std::move(device_manager));

    usb_chooser_controller_.reset(new UsbChooserController(
        main_rfh(), std::move(device_filters), std::move(callback)));
    mock_usb_chooser_view_.reset(new MockUsbChooserView());
    usb_chooser_controller_->set_view(mock_usb_chooser_view_.get());
    // Make sure the device::mojom::UsbDeviceManager::SetClient() call has
    // been received.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  device::mojom::UsbDeviceInfoPtr CreateAndAddFakeUsbDevice(
      const std::string& product_string,
      const std::string& serial_number) {
    return device_manager_.CreateAndAddDevice(0, 1, "Google", product_string,
                                              serial_number);
  }

  device::FakeUsbDeviceManager device_manager_;
  std::unique_ptr<UsbChooserController> usb_chooser_controller_;
  std::unique_ptr<MockUsbChooserView> mock_usb_chooser_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UsbChooserControllerTest);
};

TEST_F(UsbChooserControllerTest, AddDevice) {
  EXPECT_CALL(*mock_usb_chooser_view_, OnOptionAdded(0)).Times(1);
  CreateAndAddFakeUsbDevice("a", "001");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, usb_chooser_controller_->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("a"), usb_chooser_controller_->GetOption(0));

  EXPECT_CALL(*mock_usb_chooser_view_, OnOptionAdded(1)).Times(1);
  CreateAndAddFakeUsbDevice("b", "002");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, usb_chooser_controller_->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("b"), usb_chooser_controller_->GetOption(1));

  EXPECT_CALL(*mock_usb_chooser_view_, OnOptionAdded(2)).Times(1);
  CreateAndAddFakeUsbDevice("c", "003");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, usb_chooser_controller_->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("c"), usb_chooser_controller_->GetOption(2));
}

TEST_F(UsbChooserControllerTest, RemoveDevice) {
  auto device_a = CreateAndAddFakeUsbDevice("a", "001");
  auto device_b = CreateAndAddFakeUsbDevice("b", "002");
  auto device_c = CreateAndAddFakeUsbDevice("c", "003");
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*mock_usb_chooser_view_, OnOptionRemoved(1)).Times(1);
  device_manager_.RemoveDevice(device_b->guid);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, usb_chooser_controller_->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("a"), usb_chooser_controller_->GetOption(0));
  EXPECT_EQ(base::ASCIIToUTF16("c"), usb_chooser_controller_->GetOption(1));

  EXPECT_CALL(*mock_usb_chooser_view_, OnOptionRemoved(0)).Times(1);
  device_manager_.RemoveDevice(device_a->guid);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, usb_chooser_controller_->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("c"), usb_chooser_controller_->GetOption(0));

  EXPECT_CALL(*mock_usb_chooser_view_, OnOptionRemoved(0)).Times(1);
  device_manager_.RemoveDevice(device_c->guid);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, usb_chooser_controller_->NumOptions());
}

TEST_F(UsbChooserControllerTest, AddAndRemoveDeviceWithSameName) {
  auto device_a_1 = CreateAndAddFakeUsbDevice("a", "001");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::ASCIIToUTF16("a"), usb_chooser_controller_->GetOption(0));

  auto device_b = CreateAndAddFakeUsbDevice("b", "002");
  auto device_a_2 = CreateAndAddFakeUsbDevice("a", "002");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::ASCIIToUTF16("a (001)"),
            usb_chooser_controller_->GetOption(0));
  EXPECT_EQ(base::ASCIIToUTF16("b"), usb_chooser_controller_->GetOption(1));
  EXPECT_EQ(base::ASCIIToUTF16("a (002)"),
            usb_chooser_controller_->GetOption(2));

  device_manager_.RemoveDevice(device_a_1->guid);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::ASCIIToUTF16("b"), usb_chooser_controller_->GetOption(0));
  EXPECT_EQ(base::ASCIIToUTF16("a"), usb_chooser_controller_->GetOption(1));
}

TEST_F(UsbChooserControllerTest, UnknownDeviceName) {
  uint16_t vendor_id = 123;
  uint16_t product_id = 456;
  device_manager_.CreateAndAddDevice(vendor_id, product_id);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::ASCIIToUTF16("Unknown device [007b:01c8]"),
            usb_chooser_controller_->GetOption(0));
}
