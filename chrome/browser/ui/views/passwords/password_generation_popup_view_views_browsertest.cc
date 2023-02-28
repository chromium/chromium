// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_generation_popup_view_views.h"

#include <string>
#include <tuple>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/passwords/password_generation_popup_view_views.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/widget/widget.h"

namespace {

using ::testing::Bool;
using ::testing::Combine;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using TestParameterType = std::tuple<bool, bool>;

constexpr char16_t kSampleEmail[] = u"test-account@gmail.com";

class MockPasswordGenerationPopupController
    : public PasswordGenerationPopupController {
 public:
  MockPasswordGenerationPopupController() = default;
  ~MockPasswordGenerationPopupController() override = default;

  // AutofillPopupViewDelegate:
  MOCK_METHOD(void, Hide, (autofill::PopupHidingReason), (override));
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
  MOCK_METHOD(void, OnGooglePasswordManagerLinkClicked, (), (override));
  MOCK_METHOD(std::u16string, GetPrimaryAccountEmail, (), (override));
  MOCK_METHOD(GenerationUIState, state, (), (const override));
  MOCK_METHOD(bool, password_selected, (), (const override));
  MOCK_METHOD(const std::u16string&, password, (), (const override));
  MOCK_METHOD(bool, IsUserTypedPasswordWeak, (), (const override));
  MOCK_METHOD(bool, IsStateMinimized, (), (const override));
  MOCK_METHOD(const std::u16string&, HelpText, (), (const override));
  MOCK_METHOD(std::u16string, SuggestedText, (), (const override));

  base::WeakPtr<PasswordGenerationPopupController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockPasswordGenerationPopupController> weak_ptr_factory_{
      this};
};

bool IsDarkModeOn(const TestParameterType& param) {
  return std::get<0>(param);
}

bool IsBrowserLanguageRTL(const TestParameterType& param) {
  return std::get<1>(param);
}

std::string GetTestSuffix(
    const testing::TestParamInfo<TestParameterType>& param_info) {
  return base::StrCat(
      {IsDarkModeOn(param_info.param) ? "Dark" : "Light",
       IsBrowserLanguageRTL(param_info.param) ? "BrowserRTL" : "BrowserLTR"});
}

}  // namespace

// The first boolean parameter determines whether dark mode is used. The second
// parameter determines whether the browser UI is RTL.
// TODO(crbug.com/1411172): Extract the common logic between this test and
// `PopupViewViewsBrowsertest` and move to a base class.
class PasswordGenerationPopupViewBrowsertest
    : public UiBrowserTest,
      public testing::WithParamInterface<TestParameterType> {
 public:
  PasswordGenerationPopupViewBrowsertest() = default;
  ~PasswordGenerationPopupViewBrowsertest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (IsDarkModeOn(GetParam())) {
      command_line->AppendSwitch(switches::kForceDarkMode);
    }
  }

  void SetUpOnMainThread() override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    gfx::NativeView native_view = web_contents->GetNativeView();
    ON_CALL(controller_, container_view()).WillByDefault(Return(native_view));
    ON_CALL(controller_, GetWebContents()).WillByDefault(Return(web_contents));
    ON_CALL(controller_, element_bounds())
        .WillByDefault(ReturnRef(kElementBounds));

    ON_CALL(controller_, GetPrimaryAccountEmail)
        .WillByDefault(Return(kSampleEmail));
    ON_CALL(controller_, password).WillByDefault(ReturnRef(password_));

    base::i18n::SetRTLForTesting(IsBrowserLanguageRTL(GetParam()));
  }

  void PrepareOfferGenerationState() {
    ON_CALL(controller_, state)
        .WillByDefault(Return(PasswordGenerationPopupController::
                                  GenerationUIState::kOfferGeneration));
    ON_CALL(controller_, IsStateMinimized).WillByDefault(Return(false));
    ON_CALL(controller_, SuggestedText)
        .WillByDefault(Return(
            l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_SUGGESTION_GPM)));
  }

  void PrepareEditingSuggestionState() {
    ON_CALL(controller_, state)
        .WillByDefault(Return(PasswordGenerationPopupController::
                                  GenerationUIState::kEditGeneratedPassword));
    ON_CALL(controller_, IsStateMinimized).WillByDefault(Return(false));
    ON_CALL(controller_, SuggestedText)
        .WillByDefault(Return(l10n_util::GetStringUTF16(
            IDS_PASSWORD_GENERATION_EDITING_SUGGESTION)));
  }

  void PrepareMinimizedState() {
    ON_CALL(controller_, IsStateMinimized).WillByDefault(Return(true));
  }

  // Marks the popup as selected (i.e. the state it is in when a user hovers
  // over it).
  void SetSelected(bool selected) {
    ON_CALL(controller_, password_selected).WillByDefault(Return(selected));
  }

  void ShowUi(const std::string& name) override {
    view_ = new PasswordGenerationPopupViewViews(
        controller_.GetWeakPtr(), views::Widget::GetWidgetForNativeWindow(
                                      browser()->window()->GetNativeWindow()));
    ASSERT_TRUE(view_->Show());
    // If this update is not forced, the password selection state does not get
    // taken into account.
    if (!controller_.IsStateMinimized()) {
      view_->PasswordSelectionUpdated();
    }
  }

  bool VerifyUi() override {
    if (!view_) {
      return false;
    }

    views::Widget* widget = view_->GetWidget();
    if (!widget) {
      return false;
    }

    // VerifyPixelUi works only for these platforms.
    // TODO(crbug.com/958242): Revise this if supported platforms change.
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
    auto* test_info = testing::UnitTest::GetInstance()->current_test_info();
    const std::string screenshot_name =
        base::StrCat({test_info->test_case_name(), "_", test_info->name()});

    return VerifyPixelUi(widget, "PasswordGenerationPopupViewViewsBrowsertest",
                         screenshot_name);
#else
    return true;
#endif
  }

  void WaitForUserDismissal() override {}

  void TearDownOnMainThread() override {
    EXPECT_CALL(controller_, ViewDestroyed());
    view_ = nullptr;
    UiBrowserTest::TearDownOnMainThread();
  }

 private:
  static constexpr gfx::RectF kElementBounds{100, 100, 250, 50};
  const std::u16string password_{u"123!-scfFGamFD"};

  NiceMock<MockPasswordGenerationPopupController> controller_;
  raw_ptr<PasswordGenerationPopupViewViews> view_ = nullptr;
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

IN_PROC_BROWSER_TEST_P(PasswordGenerationPopupViewBrowsertest, MinimizedState) {
  PrepareMinimizedState();
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(All,
                         PasswordGenerationPopupViewBrowsertest,
                         Combine(Bool(), Bool()),
                         GetTestSuffix);
