// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/device_dialog/usb_chooser_dialog_android.h"

#include <string>

#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/usb/usb_chooser_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom-blink.h"
#include "ui/android/window_android.h"

namespace {

using UsbChooserDialogAndroidTest = ChromeRenderViewHostTestHarness;
using testing::_;

TEST_F(UsbChooserDialogAndroidTest, FrameTree) {
  NavigateAndCommit(GURL("https://main-frame.com"));
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://sub-frame.com"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("subframe"));

  auto options = blink::mojom::WebUsbRequestDeviceOptions::New();
  auto controller = std::make_unique<UsbChooserController>(
      main_rfh(), std::move(options),
      base::BindLambdaForTesting(
          [](device::mojom::UsbDeviceInfoPtr usb_device_info) {}));

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(main_rfh());
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window.get()->get()->AddChild(web_contents->GetNativeView());
  ChromeSecurityStateTabHelper::CreateForWebContents(web_contents);

  base::MockCallback<UsbChooserDialogAndroid::CreateJavaDialogCallback>
      mock_callback;
  auto origin_predicate =
      [&](const base::android::JavaRef<jstring>& java_string) {
        return base::android::ConvertJavaStringToUTF16(
                   base::android::AttachCurrentThread(), java_string) ==
               u"https://main-frame.com";
      };
  EXPECT_CALL(mock_callback, Run(/*env=*/_, /*window_android=*/_,
                                 testing::Truly(origin_predicate),
                                 /*security_level=*/_, /*profile=*/_,
                                 /*native_usb_chooser_dialog_ptr=*/_));
  UsbChooserDialogAndroid::CreateForTesting(subframe, std::move(controller),
                                            base::BindLambdaForTesting([]() {}),
                                            mock_callback.Get());
}

}  // namespace
