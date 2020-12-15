// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_tab_helper.h"

#include "chrome/browser/usb/frame_usb_services.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/test_support/decorators_utils.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/service_manager/public/cpp/test/test_service.h"
#include "services/service_manager/public/cpp/test/test_service_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "url/url_constants.h"

class UsbTabHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  UsbTabHelperTest() = default;
  ~UsbTabHelperTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    pm_harness_.SetUp();
    // Reset the test contents to ensure that it has a PageNode associated to it
    // in the PerformanceManager graph.
    SetContents(CreateTestWebContents());

    auto* chooser_context = UsbChooserContextFactory::GetForProfile(profile());
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
    device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());
    chooser_context->SetDeviceManagerForTesting(std::move(device_manager));

    NavigateAndCommit(GURL("https://www.google.com"));
  }

  void TearDown() override {
    pm_harness_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 private:
  performance_manager::PerformanceManagerTestHarnessHelper pm_harness_;
  device::FakeUsbDeviceManager device_manager_;
};

TEST_F(UsbTabHelperTest, IncrementDecrementConnectionCount) {
  mojo::Remote<blink::mojom::WebUsbService> remote;

  FrameUsbServices::CreateFrameUsbServices(main_rfh(),
                                           remote.BindNewPipeAndPassReceiver());
  UsbTabHelper* helper = UsbTabHelper::FromWebContents(web_contents());

  EXPECT_FALSE(helper->IsDeviceConnected());
  performance_manager::testing::TestPageNodePropertyOnPMSequence(
      web_contents(),
      &performance_manager::PageLiveStateDecorator::Data::
          GetOrCreateForPageNode,
      &performance_manager::PageLiveStateDecorator::Data::
          IsConnectedToUSBDevice,
      false);

  // Increment the USB connection count. Expect USBTabHelper and
  // PerformanceManager to indicate that the tab is attached to USB.
  helper->IncrementConnectionCount();
  EXPECT_TRUE(helper->IsDeviceConnected());
  performance_manager::testing::TestPageNodePropertyOnPMSequence(
      web_contents(),
      &performance_manager::PageLiveStateDecorator::Data::
          GetOrCreateForPageNode,
      &performance_manager::PageLiveStateDecorator::Data::
          IsConnectedToUSBDevice,
      true);

  // Increment the USB connection count again. State shouldn't change in
  // USBTabHelper and in the PerformanceManager.
  helper->IncrementConnectionCount();
  EXPECT_TRUE(helper->IsDeviceConnected());
  performance_manager::testing::TestPageNodePropertyOnPMSequence(
      web_contents(),
      &performance_manager::PageLiveStateDecorator::Data::
          GetOrCreateForPageNode,
      &performance_manager::PageLiveStateDecorator::Data::
          IsConnectedToUSBDevice,
      true);

  // Decrement the USB connection count. State shouldn't change in USBTabHelper
  // and in the PerformanceManager as one connection remains.
  helper->DecrementConnectionCount();
  EXPECT_TRUE(helper->IsDeviceConnected());
  performance_manager::testing::TestPageNodePropertyOnPMSequence(
      web_contents(),
      &performance_manager::PageLiveStateDecorator::Data::
          GetOrCreateForPageNode,
      &performance_manager::PageLiveStateDecorator::Data::
          IsConnectedToUSBDevice,
      true);

  // Decrement the USB connection count again. Expect USBTabHelper and
  // PerformanceManager to indicate that the tab is *not* attached to USB.
  helper->DecrementConnectionCount();
  EXPECT_FALSE(helper->IsDeviceConnected());
  performance_manager::testing::TestPageNodePropertyOnPMSequence(
      web_contents(),
      &performance_manager::PageLiveStateDecorator::Data::
          GetOrCreateForPageNode,
      &performance_manager::PageLiveStateDecorator::Data::
          IsConnectedToUSBDevice,
      false);
}
