// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_service.h"

#include "base/containers/enum_set.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_registry.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "content/public/test/browser_test.h"

namespace {
using ToastIdEnumSet =
    base::EnumSet<ToastId, ToastId::kMinValue, ToastId::kMaxValue>;
}

class ToastServiceBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {toast_features::kToastFramework, commerce::kCompareConfirmationToast,
         commerce::kProductSpecifications},
        /*disabled_features*/ {});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that all ToastIds are registered with the toast registry owned by
// the toast service.
IN_PROC_BROWSER_TEST_F(ToastServiceBrowserTest, RegisterAllToastIds) {
  ToastService* const toast_service =
      browser()->browser_window_features()->toast_service();
  const ToastRegistry* const toast_registry = toast_service->toast_registry();

  for (ToastId id : ToastIdEnumSet::All()) {
    EXPECT_NE(toast_registry->GetToastSpecification(id), nullptr);
  }
}

// Verifies that the ToastService and ToastController should exist for normal
// browser windows, and PWAs. The ToastService and ToastController should be
// null for other browser types since toasts are not supported on them.
IN_PROC_BROWSER_TEST_F(ToastServiceBrowserTest, ServiceExistForBrowserTypes) {
  BrowserWindowFeatures* const normal_window_features =
      browser()->browser_window_features();
  EXPECT_TRUE(normal_window_features->toast_service());
  EXPECT_TRUE(normal_window_features->toast_controller());
  Profile* const profile = browser()->profile();

  BrowserWindowFeatures* const popup_window_features =
      CreateBrowserForPopup(profile)->browser_window_features();
  EXPECT_FALSE(popup_window_features->toast_service());
  EXPECT_FALSE(popup_window_features->toast_controller());

  BrowserWindowFeatures* const app_window_features =
      CreateBrowserForApp("test_app_name", profile)->browser_window_features();
  EXPECT_TRUE(app_window_features->toast_service());
  EXPECT_TRUE(app_window_features->toast_controller());

  BrowserWindowFeatures* const pip_window_features =
      Browser::Create(Browser::CreateParams::CreateForPictureInPicture(
                          "test_app_name", false, profile, false))
          ->browser_window_features();
  EXPECT_FALSE(pip_window_features->toast_service());
  EXPECT_FALSE(pip_window_features->toast_controller());

  BrowserWindowFeatures* const devtools_window_features =
      Browser::Create(Browser::CreateParams::CreateForDevTools(profile))
          ->browser_window_features();
  EXPECT_FALSE(devtools_window_features->toast_service());
  EXPECT_FALSE(devtools_window_features->toast_controller());
}
