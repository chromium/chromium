// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/serial/serial_chooser_controller.h"

#include "base/test/bind_test_util.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/test/fake_serial_port_manager.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/serial/serial.mojom.h"

class SerialChooserControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    mojo::PendingRemote<device::mojom::SerialPortManager> port_manager;
    port_manager_.AddReceiver(port_manager.InitWithNewPipeAndPassReceiver());
    SerialChooserContextFactory::GetForProfile(profile())
        ->SetPortManagerForTesting(std::move(port_manager));
  }

 private:
  device::FakeSerialPortManager port_manager_;
};

TEST_F(SerialChooserControllerTest, GetPortsLateResponse) {
  std::vector<blink::mojom::SerialPortFilterPtr> filters;

  bool callback_run = false;
  auto callback = base::BindLambdaForTesting(
      [&](device::mojom::SerialPortInfoPtr port_info) {
        EXPECT_FALSE(port_info);
        callback_run = true;
      });

  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters), std::move(callback));
  controller.reset();

  // Allow any tasks posted by |controller| to run, such as asynchronous
  // requests to the Device Service to get the list of available serial ports.
  // These should be safely discarded since |controller| was destroyed.
  base::RunLoop().RunUntilIdle();

  // Even if |controller| is destroyed without user interaction the callback
  // should be run.
  EXPECT_TRUE(callback_run);
}
