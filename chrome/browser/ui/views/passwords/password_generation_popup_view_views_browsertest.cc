// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_generation_popup_view_views.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_pixel_test.h"
#include "chrome/browser/ui/views/passwords/password_generation_popup_view_views.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

namespace {

using ::testing::Bool;
using ::testing::Combine;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::Values;

constexpr char16_t kSampleEmail[] = u"test-account@gmail.com";

class MockPasswordGenerationPopupController
    : public PasswordGenerationPopupController {
 public:
  MockPasswordGenerationPopupController() = default;
  ~MockPasswordGenerationPopupController() override = default;

  // AutofillPopupViewDelegate:
  MOCK_METHOD(void, Hide, (autofill::SuggestionHidingReason), (override));
  MOCK_METHOD(void, ViewDestroyed, (), (override));
  MOCK_METHOD(gfx::NativeView, container_view, (), (const override));
  MOCK_METHOD(content::WebContents*, GetWebContents, (), (const override));
  MOCK_METHOD(const gfx::RectF&, element_bounds, (), (const override));
  MOCK_METHOD(autofill::PopupAnchorType, anchor_type, (), (const override));
  MOCK_METHOD(base::i18n::TextDirection,
              GetElementTextDirection,
              (),
              (const override));

  // PasswordGenerationPopupController:
  MOCK_METHOD(void, PasswordAccepted, (), (override));
  MOCK_METHOD(void, SetSelected, (), (override));
  MOCK_METHOD(void, SelectionCleared, (), (override));
  MOCK_METHOD(std::u16string, GetPrimaryAccountEmail, (), (override));
  MOCK_METHOD(bool, ShouldShowNudgePassword, (), (const override));
  MOCK_METHOD(GenerationUIState, state, (), (const override));
  MOCK_METHOD(bool, password_selected, (), (const override));
  MOCK_METHOD(bool, accept_button_selected, (), (const override));
  MOCK_METHOD(bool, cancel_button_selected, (), (const override));
  MOCK_METHOD(const std::u16string&, password, (), (const override));
  MOCK_METHOD(const std::u16string&, HelpText, (), (const override));
  MOCK_METHOD(std::u16string, SuggestedText, (), (const override));

  base::WeakPtr<PasswordGenerationPopupController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockPasswordGenerationPopupController> weak_ptr_factory_{
      this};
};

}  // namespace

class PasswordGenerationPopupViewBrowsertest
    : public autofill::PopupPixelTest<PasswordGenerationPopupViewViews,
                                      MockPasswordGenerationPopupController> {
 public:
  PasswordGenerationPopupViewBrowsertest() {
    // TODO(crbug.com/41492898): Clean up when launched.
    feature_list_.InitAndDisableFeature(
        password_manager::features::kPasswordGenerationSoftNudge);
  }
  ~PasswordGenerationPopupViewBrowsertest() override = default;

  void SetUpOnMainThread() override {
    PopupPixelTest::SetUpOnMainThread();

    ON_CALL(controller(), element_bounds())
        .WillByDefault(ReturnRef(kElementBounds));

    ON_CALL(controller(), GetPrimaryAccountEmail)
        .WillByDefault(Return(kSampleEmail));
    ON_CALL(controller(), password).WillByDefault(ReturnRef(password_));
  }

  void PrepareOfferGenerationState() {
    ON_CALL(controller(), state)
        .WillByDefault(Return(PasswordGenerationPopupController::
                                  GenerationUIState::kOfferGeneration));
    ON_CALL(controller(), SuggestedText)
        .WillByDefault(Return(
            l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_SUGGESTION_GPM)));
  }

  void PrepareEditingSuggestionState() {
    ON_CALL(controller(), state)
        .WillByDefault(Return(PasswordGenerationPopupController::
                                  GenerationUIState::kEditGeneratedPassword));
    ON_CALL(controller(), SuggestedText)
        .WillByDefault(Return(l10n_util::GetStringUTF16(
            IDS_PASSWORD_GENERATION_EDITING_SUGGESTION)));
  }

  // Marks the popup as selected (i.e. the state it is in when a user hovers
  // over it).
  void SetSelected(bool selected) {
    ON_CALL(controller(), password_selected).WillByDefault(Return(selected));
  }

  void ShowUi(const std::string& name) override {
    PopupPixelTest::ShowUi(name);
    ASSERT_TRUE(view()->Show());
    // If this update is not forced, the password selection state does not get
    // taken into account.
    view()->PasswordSelectionUpdated();
  }

 protected:
  // autofill::PopupPixelTest:
  PasswordGenerationPopupViewViews* CreateView(
      MockPasswordGenerationPopupController& controller) override {
    return new PasswordGenerationPopupViewViews(
        controller.GetWeakPtr(), views::Widget::GetWidgetForNativeWindow(
                                     browser()->window()->GetNativeWindow()));
  }

 private:
  static constexpr gfx::RectF kElementBounds{100, 100, 250, 50};
  const std::u16string password_{u"123!-scfFGamFD"};
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(PasswordGenerationPopupViewBrowsertest,
                       OfferPasswordGeneration) {
  PrepareOfferGenerationState();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordGenerationPopupViewBrowsertest,
                       OfferPasswordGenerationHovered) {
  PrepareOfferGenerationState();
  SetSelected(true);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordGenerationPopupViewBrowsertest,
                       EditingSuggestionState) {
  PrepareEditingSuggestionState();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(PasswordGenerationPopupViewBrowsertest,
                       EditingSuggestionStateHovered) {
  PrepareEditingSuggestionState();
  SetSelected(true);
  ShowAndVerifyUi();
}

// The test parameters define whether:
// * dark mode is enabled
// * browser language RTL is enabled
INSTANTIATE_TEST_SUITE_P(All,
                         PasswordGenerationPopupViewBrowsertest,
                         Combine(Bool(), Bool()),
                         PasswordGenerationPopupViewBrowsertest::GetTestSuffix);

// TODO(crbug.com/41492898): Remove once
class PasswordGenerationPopupViewWithSoftNudgeBrowsertest
    : public autofill::PopupPixelTest<PasswordGenerationPopupViewViews,
                                      MockPasswordGenerationPopupController> {
 public:
  PasswordGenerationPopupViewWithSoftNudgeBrowsertest() {
    feature_list_.InitAndEnableFeature(
        password_manager::features::kPasswordGenerationSoftNudge);
  }
  ~PasswordGenerationPopupViewWithSoftNudgeBrowsertest() override = default;

  void SetUpOnMainThread() override {
    PopupPixelTest::SetUpOnMainThread();

    ON_CALL(controller(), element_bounds())
        .WillByDefault(ReturnRef(kElementBounds));

    ON_CALL(controller(), GetPrimaryAccountEmail)
        .WillByDefault(Return(kSampleEmail));
    ON_CALL(controller(), password).WillByDefault(ReturnRef(password_));
  }

  void PrepareOfferGenerationState() {
    ON_CALL(controller(), state)
        .WillByDefault(Return(PasswordGenerationPopupController::
                                  GenerationUIState::kOfferGeneration));
    ON_CALL(controller(), SuggestedText)
        .WillByDefault(Return(
            l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_SUGGESTION_GPM)));
    ON_CALL(controller(), ShouldShowNudgePassword).WillByDefault(Return(true));
  }

  void ShowUi(const std::string& name) override {
    PopupPixelTest::ShowUi(name);
    ASSERT_TRUE(view()->Show());
  }

 protected:
  // autofill::PopupPixelTest:
  PasswordGenerationPopupViewViews* CreateView(
      MockPasswordGenerationPopupController& controller) override {
    return new PasswordGenerationPopupViewViews(
        controller.GetWeakPtr(), views::Widget::GetWidgetForNativeWindow(
                                     browser()->window()->GetNativeWindow()));
  }

 private:
  static constexpr gfx::RectF kElementBounds{100, 100, 250, 50};
  const std::u16string password_{u"123!-scfFGamFD"};
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(PasswordGenerationPopupViewWithSoftNudgeBrowsertest,
                       OfferPasswordGeneration) {
  PrepareOfferGenerationState();
  ShowAndVerifyUi();
}

// The test parameters define whether:
// * dark mode is enabled
// * browser language RTL is enabled
INSTANTIATE_TEST_SUITE_P(All,
                         PasswordGenerationPopupViewWithSoftNudgeBrowsertest,
                         Combine(Bool(), Bool()),
                         PasswordGenerationPopupViewBrowsertest::GetTestSuffix);
