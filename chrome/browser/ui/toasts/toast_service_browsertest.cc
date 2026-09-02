// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_service.h"

#include "base/containers/enum_set.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_registry.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/data_sharing/public/features.h"
#include "components/multistep_filter/core/features.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_test.h"

namespace {

using ToastIdEnumSet = base::EnumSet<ToastId>;

// Toast IDs that have been deprecated and no longer have a registered
// specification.
constexpr auto kDeprecatedToastIds =
    std::to_array<std::underlying_type_t<ToastId>>(
        {/*kLensOverlay=*/4, /*kAddedToComparisonTable=*/6,
         /*kPlusAddressOverride=*/8, /*kMultistepFilterSuggestion=*/31,
         /*kMultistepFilterSuggestionRecent=*/32});

ToastIdEnumSet GetActiveToastIds() {
  auto result = ToastIdEnumSet::All();
  for (auto toast_id : kDeprecatedToastIds) {
    result.Remove(static_cast<ToastId>(toast_id));
  }
#if BUILDFLAG(IS_CHROMEOS)
  result.Remove(ToastId::kDefaultBrowserUpdateSuccess);
#endif
  return result;
}

class ToastServiceBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{autofill::features::kAutofillAiWalletPrivatePasses, {}},
         {safe_browsing::kEsbAsASyncedSetting, {}},
         {data_sharing::features::kDataSharingFeature, {}},
         {toast_features::kTranslateToast, {}},
         {features::kGlicActorUi, {{features::kGlicActorUiToastName, "true"}}},
         {multistep_filter::kMultistepFilter, {}},
         {features::kIndigo, {}},
         {autofill::features::kAutofillAmbientAutofill, {}},
         {autofill::features::kAutofillAtMemory, {}},
         {dictation::kDictation, {}}},
        /*disabled_features*/ {});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that all ToastIds are registered with the toast registry owned by
// the toast service.
IN_PROC_BROWSER_TEST_F(ToastServiceBrowserTest, RegisterAllToastIds) {
  ToastService* const toast_service = ToastService::From(browser());
  const ToastRegistry* const toast_registry = toast_service->toast_registry();

  for (ToastId id : GetActiveToastIds()) {
    EXPECT_NE(toast_registry->GetToastSpecification(id), nullptr);
  }
}

// Verifies that the ToastService and ToastController should exist for normal
// browser windows, and PWAs. The ToastService and ToastController should be
// null for other browser types since toasts are not supported on them.
IN_PROC_BROWSER_TEST_F(ToastServiceBrowserTest, ServiceExistForBrowserTypes) {
  EXPECT_TRUE(ToastService::From(browser()));
  EXPECT_TRUE(ToastController::From(browser()));
  Profile* const profile = browser()->GetProfile();

  BrowserWindowInterface* const popup_browser = CreateBrowserForPopup(profile);
  EXPECT_FALSE(ToastService::From(popup_browser));
  EXPECT_FALSE(ToastController::From(popup_browser));

  BrowserWindowInterface* const app_browser =
      CreateBrowserForApp("test_app_name", profile);
  EXPECT_TRUE(ToastService::From(app_browser));
  EXPECT_TRUE(ToastController::From(app_browser));

  BrowserWindowInterface* const pip_browser =
      CreateBrowserWindow(BrowserWindowCreateParams::CreateForPictureInPicture(
          "test_app_name", /*trusted_source=*/false, profile,
          /*user_gesture=*/false));
  AddBlankTabAndShow(pip_browser);
  EXPECT_FALSE(ToastService::From(pip_browser));
  EXPECT_FALSE(ToastController::From(pip_browser));

  BrowserWindowInterface* const devtools_browser = CreateBrowserWindow(
      BrowserWindowCreateParams::CreateForDevTools(profile));
  AddBlankTabAndShow(devtools_browser);
  EXPECT_FALSE(ToastService::From(devtools_browser));
  EXPECT_FALSE(ToastController::From(devtools_browser));
}

}  // namespace
