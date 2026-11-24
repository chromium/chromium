// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_service.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/action_variants_reader.h"
#include "base/test/metrics/histogram_enum_reader.h"
#include "base/test/metrics/histogram_variants_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_registry.h"
#include "chrome/browser/ui/toasts/api/toast_specification.h"
#include "chrome/browser/ui/toasts/test/mock_toast_controller.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/data_sharing/public/features.h"
#include "components/multistep_filter/core/features.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

namespace {

using ToastIdEnumSet = base::EnumSet<ToastId>;

// Toast IDs that have been deprecated and no longer have a registered
// specification.
constexpr auto kDeprecatedToastIds =
    std::to_array<std::underlying_type_t<ToastId>>(
        {/*kLensOverlay=*/4, /*kAddedToComparisonTable=*/6,
         /*kPlusAddressOverride=*/8, /*kMultistepFilterSuggestion=*/31,
         /*kMultistepFilterSuggestionRecent=*/32});

ToastIdEnumSet GetNonDeprecatedToastIds() {
  auto result = ToastIdEnumSet::All();
  for (auto toast_id : kDeprecatedToastIds) {
    result.Remove(static_cast<ToastId>(toast_id));
  }
  return result;
}

ToastIdEnumSet GetActiveToastIds() {
  auto result = GetNonDeprecatedToastIds();
#if BUILDFLAG(IS_CHROMEOS)
  result.Remove(ToastId::kDefaultBrowserUpdateSuccess);
#endif
#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_LINUX)
  result.Remove(ToastId::kScheduledRestartOnIdle);
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
         {autofill::features::kAutofillAmbientAutofillSuppression, {}},
         {autofill::features::kAutofillAtMemory, {}},
         {dictation::kDictation, {}},
         {ttc::kTtc, {}}},
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

// Verifies that every ToastId (including deprecated entries) has an entry in
// the ToastId enum in //tools/metrics/histograms/metadata/toasts/enums.xml.
IN_PROC_BROWSER_TEST_F(ToastServiceBrowserTest, AllToastIdsInEnumsXml) {
  std::optional<base::HistogramEnumEntryMap> enum_entries;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    enum_entries = base::ReadEnumFromEnumsXml("ToastId", "toasts");
  }
  ASSERT_TRUE(enum_entries.has_value())
      << "Failed to read ToastId enum from "
         "//tools/metrics/histograms/metadata/toasts/enums.xml";

  for (ToastId id : ToastIdEnumSet::All()) {
    EXPECT_TRUE(enum_entries->contains(std::to_underlying(id)))
        << "ToastId (" << std::to_underlying(id)
        << ") is missing from the ToastId enum in "
           "//tools/metrics/histograms/metadata/toasts/enums.xml";
  }
}

// Verifies that every active ToastId has a corresponding variant matching
// GetToastName() in the ToastName variants list in
// //tools/metrics/histograms/metadata/toasts/histograms.xml.
IN_PROC_BROWSER_TEST_F(ToastServiceBrowserTest, AllToastIdsInHistogramsXml) {
  std::optional<base::HistogramVariantsEntryMap> variants;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    variants = base::ReadVariantsFromHistogramsXml("ToastName", "toasts");
  }
  ASSERT_TRUE(variants.has_value())
      << "Failed to read ToastName variants from "
         "//tools/metrics/histograms/metadata/toasts/histograms.xml";

  for (ToastId id : GetNonDeprecatedToastIds()) {
    const std::string toast_name(GetToastName(id));
    EXPECT_TRUE(variants->contains(toast_name))
        << "Toast \"" << toast_name
        << "\" is missing from ToastName variants in "
           "//tools/metrics/histograms/metadata/toasts/histograms.xml";
  }
}

// Verifies that every registered toast with an action button or close button
// has a corresponding variant in //tools/metrics/actions/actions.xml.
IN_PROC_BROWSER_TEST_F(ToastServiceBrowserTest, InteractiveToastsInActionsXml) {
  std::vector<base::test::ActionVariantsEntryMap> action_button_variants;
  std::vector<base::test::ActionVariantsEntryMap> close_button_variants;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    action_button_variants = base::test::ReadActionVariantsForAction(
        "Toast.ActionButtonClicked", ".");
    close_button_variants = base::test::ReadActionVariantsForAction(
        "Toast.CloseButtonClicked", ".");
  }
  ASSERT_EQ(action_button_variants.size(), 1u)
      << "Failed to read Toast.ActionButtonClicked variants from "
         "//tools/metrics/actions/actions.xml";
  ASSERT_EQ(close_button_variants.size(), 1u)
      << "Failed to read Toast.CloseButtonClicked variants from "
         "//tools/metrics/actions/actions.xml";

  ToastService* const toast_service = ToastService::From(browser());
  const ToastRegistry* const toast_registry = toast_service->toast_registry();

  for (ToastId id : GetActiveToastIds()) {
    const ToastSpecification* const spec =
        toast_registry->GetToastSpecification(id);
    ASSERT_NE(spec, nullptr);
    const std::string toast_name(GetToastName(id));
    if (spec->action_button_string_id().has_value()) {
      EXPECT_TRUE(action_button_variants[0].contains(toast_name))
          << "Toast \"" << toast_name
          << "\" has an action button but is missing a '." << toast_name
          << "' variant for Toast.ActionButtonClicked in "
             "//tools/metrics/actions/actions.xml";
    }
    if (spec->has_close_button()) {
      EXPECT_TRUE(close_button_variants[0].contains(toast_name))
          << "Toast \"" << toast_name
          << "\" has a close button but is missing a '." << toast_name
          << "' variant for Toast.CloseButtonClicked in "
             "//tools/metrics/actions/actions.xml";
    }
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

class ToastControllerFactoryBrowserTest : public InProcessBrowserTest {
 public:
  ToastControllerFactoryBrowserTest() {
    toast_controller_override_ =
        BrowserWindowFeatures::GetUserDataFactoryForTesting()
            .AddOverrideForTesting(base::BindRepeating(
                &ToastControllerFactoryBrowserTest::CreateToastController,
                base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    last_created_controller_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  raw_ptr<MockToastController> last_created_controller_ = nullptr;

 private:
  std::unique_ptr<ToastController> CreateToastController(
      BrowserWindowInterface& browser) {
    auto controller = std::make_unique<testing::NiceMock<MockToastController>>(
        &browser, ToastService::From(&browser)->toast_registry());
    last_created_controller_ = controller.get();
    return controller;
  }

  ui::UserDataFactory::ScopedOverride toast_controller_override_;
};

IN_PROC_BROWSER_TEST_F(ToastControllerFactoryBrowserTest,
                       UsesInjectedControllerForNormalAndAppWindows) {
  auto* normal_controller = last_created_controller_.get();
  ASSERT_TRUE(normal_controller);
  ASSERT_EQ(normal_controller, ToastController::From(browser()));
  EXPECT_CALL(*normal_controller,
              MaybeShowToast(
                  testing::Field(&ToastParams::toast_id, ToastId::kLinkCopied)))
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(ToastController::From(browser())->MaybeShowToast(
      ToastParams(ToastId::kLinkCopied)));

  BrowserWindowInterface* app_browser =
      CreateBrowserForApp("test_app_name", browser()->GetProfile());
  auto* app_controller = last_created_controller_.get();
  ASSERT_TRUE(app_controller);
  ASSERT_NE(normal_controller, app_controller);
  ASSERT_EQ(app_controller, ToastController::From(app_browser));
  EXPECT_CALL(*app_controller,
              MaybeShowToast(
                  testing::Field(&ToastParams::toast_id, ToastId::kLinkCopied)))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(ToastController::From(app_browser)
                  ->MaybeShowToast(ToastParams(ToastId::kLinkCopied)));
  EXPECT_EQ(normal_controller, ToastController::From(browser()));
}

}  // namespace
