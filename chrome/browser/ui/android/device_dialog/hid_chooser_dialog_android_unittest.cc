// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/device_dialog/hid_chooser_dialog_android.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/hid/hid_chooser_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"
#include "ui/android/window_android.h"

namespace {

using HidChooserDialogAndroidTest = ChromeRenderViewHostTestHarness;
using testing::_;

TEST_F(HidChooserDialogAndroidTest, FrameTree) {
  NavigateAndCommit(GURL("https://main-frame.com"));
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://sub-frame.com"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("subframe"));

  std::vector<blink::mojom::HidDeviceFilterPtr> filters;
  std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters;
  auto controller = std::make_unique<HidChooserController>(
      main_rfh(), std::move(filters), std::move(exclusion_filters),
      base::BindLambdaForTesting(
          [](std::vector<device::mojom::HidDeviceInfoPtr> devices) {}));

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(main_rfh());
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window.get()->get()->AddChild(web_contents->GetNativeView());

  base::MockCallback<HidChooserDialogAndroid::CreateJavaDialogCallback>
      mock_callback;
  auto origin_predicate = [&](const std::u16string& java_string) {
    return java_string == u"https://main-frame.com";
  };
  EXPECT_CALL(mock_callback, Run(/*env=*/_, /*window_android=*/_,
                                 testing::Truly(origin_predicate),
                                 /*security_level=*/_, /*profile=*/_,
                                 /*native_hid_chooser_dialog_ptr=*/_));
  HidChooserDialogAndroid::CreateForTesting(subframe, std::move(controller),
                                            mock_callback.Get());
}

}  // namespace
