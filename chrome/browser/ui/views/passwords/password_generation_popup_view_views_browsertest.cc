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
  MOCK_METHOD(base::i18n::TextDirection,
              GetElementTextDirection,
              (),
              (const override));

  // PasswordGenerationPopupController:
  MOCK_METHOD(void, PasswordAccepted, (), (override));
  MOCK_METHOD(void, SetSelected, (), (override));
  MOCK_METHOD(void, SelectionCleared, (), (override));
  MOCK_METHOD(void, EditPasswordClicked, (), (override));
  MOCK_METHOD(void, EditPasswordHovered, (bool), (override));
  MOCK_METHOD(std::u16string, GetPrimaryAccountEmail, (), (override));
  MOCK_METHOD(bool, ShouldShowNudgePassword, (), (const override));
  MOCK_METHOD(GenerationUIState, state, (), (const override));
  MOCK_METHOD(bool, password_selected, (), (const override));
  MOCK_METHOD(bool, edit_password_selected, (), (const override));
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
    // TODO(crbug.com/326949412): Remove once the experiment concludes.
    feature_list_.InitAndDisableFeature(
        password_manager::features::kPasswordGenerationExperiment);
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

INSTANTIATE_TEST_SUITE_P(All,
                         PasswordGenerationPopupViewBrowsertest,
                         Combine(Bool(), Bool()),
                         PasswordGenerationPopupViewBrowsertest::GetTestSuffix);

using ExperimentParameterType = std::tuple<bool, bool, std::string>;

// TODO(crbug.com/326949412): Remove once the experiments conclude.
class PasswordGenerationPopupViewWithExperimentsBrowsertest
    : public autofill::PopupPixelTest<PasswordGenerationPopupViewViews,
                                      MockPasswordGenerationPopupController,
                                      ExperimentParameterType> {
 public:
  PasswordGenerationPopupViewWithExperimentsBrowsertest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{password_manager::features::
                                   kPasswordGenerationExperiment,
                               {{"password_generation_variation",
                                 std::get<2>(GetParam())}}}},
        /*disabled_features=*/{});
  }
  ~PasswordGenerationPopupViewWithExperimentsBrowsertest() override = default;

  static std::string GetExperimentTestSuffix(
      const testing::TestParamInfo<ExperimentParameterType>& param_info) {
    return base::StrCat(
        {std::get<0>(param_info.param) ? "Dark" : "Light",
         std::get<1>(param_info.param) ? "BrowserRTL" : "BrowserLTR",
         std::get<2>(param_info.param)});
  }

  void SetUpOnMainThread() override {
    PopupPixelTest::SetUpOnMainThread();

    ON_CALL(controller(), element_bounds())
        .WillByDefault(ReturnRef(kElementBounds));

    ON_CALL(controller(), GetPrimaryAccountEmail)
        .WillByDefault(Return(kSampleEmail));
    ON_CALL(controller(), password).WillByDefault(ReturnRef(password_));
  }

  void PrepareOfferGenerationState(const std::string& experiment) {
    ON_CALL(controller(), state)
        .WillByDefault(Return(PasswordGenerationPopupController::
                                  GenerationUIState::kOfferGeneration));

    int message_id = IDS_PASSWORD_GENERATION_SUGGESTION_GPM;
    if (experiment == "trusted_advice") {
      message_id = IDS_PASSWORD_GENERATION_SUGGESTION_TRUSTED_ADVICE;
    } else if (experiment == "safety_first") {
      message_id = IDS_PASSWORD_GENERATION_SUGGESTION_SAFETY_FIRST;
    } else if (experiment == "try_something_new") {
      message_id = IDS_PASSWORD_GENERATION_SUGGESTION_TRY_SOMETHING_NEW;
    } else if (experiment == "convenience") {
      message_id = IDS_PASSWORD_GENERATION_SUGGESTION_CONVENIENCE;
    }
    ON_CALL(controller(), SuggestedText)
        .WillByDefault(Return(l10n_util::GetStringUTF16(message_id)));

    if (experiment == "nudge_password") {
      ON_CALL(controller(), ShouldShowNudgePassword)
          .WillByDefault(Return(true));
    }
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

IN_PROC_BROWSER_TEST_P(PasswordGenerationPopupViewWithExperimentsBrowsertest,
                       OfferPasswordGeneration) {
  PrepareOfferGenerationState(std::get<2>(GetParam()));
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         PasswordGenerationPopupViewWithExperimentsBrowsertest,
                         Combine(Bool(),
                                 Bool(),
                                 Values("trusted_advice",
                                        "safety_first",
                                        "try_something_new",
                                        "convenience",
                                        "cross_device",
                                        "edit_password",
                                        "chunk_password",
                                        "nudge_password")),
                         PasswordGenerationPopupViewWithExperimentsBrowsertest::
                             GetExperimentTestSuffix);
