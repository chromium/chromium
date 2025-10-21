// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_SAVE_OR_UPDATE_AUTOFILL_AI_DATA_BUBBLE_VIEW_BROWSERTEST_CC_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_SAVE_OR_UPDATE_AUTOFILL_AI_DATA_BUBBLE_VIEW_BROWSERTEST_CC_H_

#include "chrome/browser/ui/views/autofill/autofill_ai/autofill_ai_import_data_bubble_view.h"

#include <tuple>
#include <utility>

#include "base/strings/strcat.h"
#include "chrome/browser/ui/autofill/autofill_ai/mock_autofill_ai_import_data_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace autofill {
namespace {

using ::testing::Bool;
using ::testing::Combine;
using EntityAttributeUpdateDetails =
    AutofillAiImportDataController::EntityAttributeUpdateDetails;
using EntityAttributeUpdateType =
    AutofillAiImportDataController::EntityAttributeUpdateType;
using TestParameterType = std::tuple<bool, bool>;

class AutofillAiImportDataBubbleViewBrowsertest
    : public UiBrowserTest,
      public testing::WithParamInterface<TestParameterType> {
 public:
  AutofillAiImportDataBubbleViewBrowsertest() = default;
  ~AutofillAiImportDataBubbleViewBrowsertest() override = default;

  // BrowserTestBase:
  void SetUpOnMainThread() override {
    UiBrowserTest::SetUpOnMainThread();

    base::i18n::SetRTLForTesting(IsBrowserLanguageRTL(this->GetParam()));
    ON_CALL(mock_controller(), GetTitleImagesResourceId())
        .WillByDefault(testing::Return(
            IDR_AUTOFILL_SAVE_PASSPORT_AND_NATIONAL_ID_CARD_LOTTIE));
  }

  void DismissUi() override { bubble_ = nullptr; }

  static bool IsDarkModeOn(const TestParameterType& param) {
    return std::get<0>(param);
  }
  static bool IsBrowserLanguageRTL(const TestParameterType& param) {
    return std::get<1>(param);
  }

  static std::string GetTestSuffix(
      const testing::TestParamInfo<TestParameterType>& param_info) {
    return base::StrCat(
        {IsDarkModeOn(param_info.param) ? "Dark" : "Light",
         IsBrowserLanguageRTL(param_info.param) ? "BrowserRTL" : "BrowserLTR"});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (IsDarkModeOn(this->GetParam())) {
      command_line->AppendSwitch(switches::kForceDarkMode);
    }
  }

  void ShowUi(const std::string& name) override {
    auto bubble = std::make_unique<AutofillAiImportDataBubbleView>(
        nullptr, browser()->tab_strip_model()->GetActiveWebContents(),
        &mock_controller());
    bubble->set_has_parent(false);
    bubble_ = bubble.get();
    views::BubbleDialogDelegateView::CreateBubble(std::move(bubble))->Show();
  }

  bool VerifyUi() override {
    if (!bubble_) {
      return false;
    }

    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    return VerifyPixelUi(bubble_->GetWidget(), test_info->test_suite_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {}

  MockAutofillAiImportDataController& mock_controller() {
    return mock_controller_;
  }

 private:
  raw_ptr<AutofillAiImportDataBubbleView> bubble_ = nullptr;
  testing::NiceMock<MockAutofillAiImportDataController> mock_controller_;
};

// `TypicalPassportCase` here and in other test(s) means that this test creates
// a dialog that is close to what most users will see.
IN_PROC_BROWSER_TEST_P(AutofillAiImportDataBubbleViewBrowsertest,
                       TypicalPassportCase_Save) {
  ON_CALL(mock_controller(), GetDialogTitle())
      .WillByDefault(testing::Return(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE)));
  std::vector<EntityAttributeUpdateDetails> details = {
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Name", /*attribute_value=*/u"Jon doe",
          EntityAttributeUpdateType::kNewEntityAttributeAdded),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Country", /*attribute_value=*/u"Brazil",
          EntityAttributeUpdateType::kNewEntityAttributeAdded),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Expiry date",
          /*attribute_value=*/u"12/12/2027",
          EntityAttributeUpdateType::kNewEntityAttributeAdded),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Issue date",
          /*attribute_value=*/u"12/12/2020",
          EntityAttributeUpdateType::kNewEntityAttributeAdded)};
  ON_CALL(mock_controller(), GetUpdatedAttributesDetails())
      .WillByDefault(testing::Return(details));
  ON_CALL(mock_controller(), IsSavePrompt())
      .WillByDefault(testing::Return(true));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(AutofillAiImportDataBubbleViewBrowsertest,
                       TypicalPassportCase_Update) {
  ON_CALL(mock_controller(), GetDialogTitle())
      .WillByDefault(testing::Return(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE)));
  std::vector<EntityAttributeUpdateDetails> details = {
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Name", /*attribute_value=*/u"Jon doe",
          EntityAttributeUpdateType::kNewEntityAttributeUpdated),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Country", /*attribute_value=*/u"Brazil",
          EntityAttributeUpdateType::kNewEntityAttributeAdded),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Expiry date",
          /*attribute_value=*/u"12/12/2027",
          EntityAttributeUpdateType::kNewEntityAttributeUnchanged),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Issue date",
          /*attribute_value=*/u"12/12/2020",
          EntityAttributeUpdateType::kNewEntityAttributeUnchanged)};
  ON_CALL(mock_controller(), GetUpdatedAttributesDetails())
      .WillByDefault(testing::Return(details));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(AutofillAiImportDataBubbleViewBrowsertest,
                       WalletableEntity_Save) {
  ON_CALL(mock_controller(), GetDialogTitle())
      .WillByDefault(testing::Return(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE)));
  ON_CALL(mock_controller(), IsSavePrompt())
      .WillByDefault(testing::Return(true));
  ON_CALL(mock_controller(), IsWalletableEntity())
      .WillByDefault(testing::Return(true));
  ON_CALL(mock_controller(), GetPrimaryAccountEmail())
      .WillByDefault(testing::Return(u"machadodeassis@gmail.com"));
  std::vector<EntityAttributeUpdateDetails> details = {
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Owner",
          /*attribute_value=*/u"Machado de Assis",
          EntityAttributeUpdateType::kNewEntityAttributeAdded),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Model",
          /*attribute_value=*/u"Käfer",
          EntityAttributeUpdateType::kNewEntityAttributeAdded),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Maker",
          /*attribute_value=*/u"Volkswagen",
          EntityAttributeUpdateType::kNewEntityAttributeAdded)};
  ON_CALL(mock_controller(), GetUpdatedAttributesDetails())
      .WillByDefault(testing::Return(details));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(AutofillAiImportDataBubbleViewBrowsertest,
                       WalletableEntity_Update) {
  ON_CALL(mock_controller(), GetDialogTitle())
      .WillByDefault(testing::Return(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_UPDATE_VEHICLE_ENTITY_DIALOG_TITLE)));
  ON_CALL(mock_controller(), IsSavePrompt())
      .WillByDefault(testing::Return(false));
  ON_CALL(mock_controller(), IsWalletableEntity())
      .WillByDefault(testing::Return(true));
  ON_CALL(mock_controller(), GetPrimaryAccountEmail())
      .WillByDefault(testing::Return(u"machadodeassis@gmail.com"));
  std::vector<EntityAttributeUpdateDetails> details = {
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Owner",
          /*attribute_value=*/u"Machado de Assis",
          EntityAttributeUpdateType::kNewEntityAttributeUpdated),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Model",
          /*attribute_value=*/u"Käfer",
          EntityAttributeUpdateType::kNewEntityAttributeUnchanged),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Maker",
          /*attribute_value=*/u"Volkswagen",
          EntityAttributeUpdateType::kNewEntityAttributeUnchanged)};
  ON_CALL(mock_controller(), GetUpdatedAttributesDetails())
      .WillByDefault(testing::Return(details));
  ShowAndVerifyUi();
}

// This tests corner cases related to attribute names and values sizes.
IN_PROC_BROWSER_TEST_P(AutofillAiImportDataBubbleViewBrowsertest,
                       LongAttributeNamesAndValues_Update) {
  ON_CALL(mock_controller(), GetDialogTitle())
      .WillByDefault(testing::Return(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE)));
  std::vector<EntityAttributeUpdateDetails> details = {
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Name", /*attribute_value=*/
          u"Jon Doe Schmidt Muller Benedikt Da Silva Mendes",
          EntityAttributeUpdateType::kNewEntityAttributeUpdated),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Country", /*attribute_value=*/u"Brazil",
          EntityAttributeUpdateType::kNewEntityAttributeAdded),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Number",
          /*attribute_value=*/
          u"123456789123456789123456789123456789123456789123456789",
          EntityAttributeUpdateType::kNewEntityAttributeAdded),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Expiry date, meaning date when your passport is "
                             u"no longer valid",
          /*attribute_value=*/
          u"December twenty four of one two thousand forty four",
          EntityAttributeUpdateType::kNewEntityAttributeUnchanged),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Issue date",
          /*attribute_value=*/u"12/12/2020",
          EntityAttributeUpdateType::kNewEntityAttributeUnchanged)};
  ON_CALL(mock_controller(), GetUpdatedAttributesDetails())
      .WillByDefault(testing::Return(details));
  ShowAndVerifyUi();
}

// This tests corner cases related to attribute names and values sizes.
IN_PROC_BROWSER_TEST_P(AutofillAiImportDataBubbleViewBrowsertest,
                       LongAttributeNamesAndValues_Save) {
  ON_CALL(mock_controller(), GetDialogTitle())
      .WillByDefault(testing::Return(l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE)));
  ON_CALL(mock_controller(), IsSavePrompt())
      .WillByDefault(testing::Return(true));
  std::vector<EntityAttributeUpdateDetails> details = {
      // Test only the value being long.
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Name",
          /*attribute_value=*/u"Jon Doe Schmidt Muller Neuhaus Sigmund Bring",
          EntityAttributeUpdateType::kNewEntityAttributeAdded),
      // Test only the attribute name being long.
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Country of birth or country where passport was "
                             u"issued",
          /*attribute_value=*/u"Brazil",
          EntityAttributeUpdateType::kNewEntityAttributeAdded),
      // Test the value being long but without a space to define the line
      // breaking point.
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Number",
          /*attribute_value=*/u"123456789123456789123456789123456789",
          EntityAttributeUpdateType::kNewEntityAttributeAdded),
      // Test both the attribute name and value being long. Note that, date
      // values are never expressed this way,
      // this is only for test purposes.
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Expiry date, meaning date when your passport is "
                             u"no longer valid",
          /*attribute_value=*/
          u"December twenty four of one two thousand forty four",
          EntityAttributeUpdateType::kNewEntityAttributeAdded),
      EntityAttributeUpdateDetails(
          /*attribute_name=*/u"Issue date",
          /*attribute_value=*/u"12/12/2020",
          EntityAttributeUpdateType::kNewEntityAttributeAdded)};
  ON_CALL(mock_controller(), GetUpdatedAttributesDetails())
      .WillByDefault(testing::Return(details));
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AutofillAiImportDataBubbleViewBrowsertest,
    Combine(/*is_dark_mode=*/Bool(), /*is_rtl=*/Bool()),
    AutofillAiImportDataBubbleViewBrowsertest::GetTestSuffix);

}  // namespace
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_SAVE_OR_UPDATE_AUTOFILL_AI_DATA_BUBBLE_VIEW_BROWSERTEST_CC_H_
