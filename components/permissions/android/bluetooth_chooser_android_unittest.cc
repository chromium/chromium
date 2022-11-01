// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/bluetooth_chooser_android.h"

#include <string>

#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "components/permissions/android/bluetooth_chooser_android_delegate.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

namespace permissions {

namespace {

using BluetoothChooserAndroidTest = content::RenderViewHostTestHarness;
using testing::_;

class FakeBluetoothChooserAndroidDelegate
    : public BluetoothChooserAndroidDelegate {
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override {
    return base::android::ScopedJavaLocalRef<jobject>();
  }
  security_state::SecurityLevel GetSecurityLevel(
      content::WebContents* web_contents) override {
    return security_state::NONE;
  }
};

TEST_F(BluetoothChooserAndroidTest, FrameTree) {
  NavigateAndCommit(GURL("https://main-frame.com"));
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://sub-frame.com"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("subframe"));

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(main_rfh());
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  window.get()->get()->AddChild(web_contents->GetNativeView());

  base::MockCallback<BluetoothChooserAndroid::CreateJavaDialogCallback>
      mock_callback;
  auto origin_predicate =
      [&](const base::android::JavaRef<jstring>& java_string) {
        return base::android::ConvertJavaStringToUTF16(
                   base::android::AttachCurrentThread(), java_string) ==
               u"https://main-frame.com";
      };
  EXPECT_CALL(mock_callback, Run(/*env=*/_, /*window_android=*/_,
                                 testing::Truly(origin_predicate),
                                 /*security_level=*/_, /*delegate=*/_,
                                 /*native_bluetooth_chooser_dialog_ptr=*/_));

  BluetoothChooserAndroid::CreateForTesting(
      subframe,
      base::BindLambdaForTesting([](content::BluetoothChooserEvent evt,
                                    const std::string& opt_device_id) {}),
      std::make_unique<FakeBluetoothChooserAndroidDelegate>(),
      mock_callback.Get());
}

}  // namespace

}  // namespace permissions
