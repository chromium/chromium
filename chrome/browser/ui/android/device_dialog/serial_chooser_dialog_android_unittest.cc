// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/device_dialog/serial_chooser_dialog_android.h"

#include <string>

#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/ui/serial/serial_chooser_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/serial/serial.mojom-forward.h"
#include "ui/android/window_android.h"

namespace {

using SerialChooserDialogAndroidTest = ChromeRenderViewHostTestHarness;
using testing::_;

TEST_F(SerialChooserDialogAndroidTest, FrameTree) {
  NavigateAndCommit(GURL("https://main-frame.com"));
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://sub-frame.com"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("subframe"));

  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  std::vector<device::BluetoothUUID> allowed_bluetooth_service_class_ids;
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters),
      std::move(allowed_bluetooth_service_class_ids),
      base::BindLambdaForTesting(
          [](device::mojom::SerialPortInfoPtr serial_port_info) {}));

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(main_rfh());
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window.get()->get()->AddChild(web_contents->GetNativeView());
  ChromeSecurityStateTabHelper::CreateForWebContents(web_contents);

  base::MockCallback<SerialChooserDialogAndroid::CreateJavaDialogCallback>
      mock_callback;
  auto origin_predicate = [&](const std::u16string& java_string) {
    return java_string == u"https://main-frame.com";
  };
  EXPECT_CALL(mock_callback, Run(/*env=*/_, /*window_android=*/_,
                                 testing::Truly(origin_predicate),
                                 /*security_level=*/_, /*profile=*/_,
                                 /*native_serial_chooser_dialog_ptr=*/_));
  SerialChooserDialogAndroid::CreateForTesting(
      subframe, std::move(controller), base::BindLambdaForTesting([]() {}),
      mock_callback.Get());
}

}  // namespace
