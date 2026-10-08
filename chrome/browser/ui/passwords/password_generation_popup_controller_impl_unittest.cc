// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"

#include <memory>
#include <string>

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller.h"
#include "chrome/browser/ui/passwords/password_generation_popup_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/accessibility/platform/assistive_tech.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect_f.h"

namespace password_manager {
namespace {

using ::autofill::password_generation::PasswordGenerationType;
using ::autofill::password_generation::PasswordGenerationUIData;
using ::testing::_;
using ::testing::Return;

PasswordGenerationUIData CreatePasswordGenerationUIData() {
  return PasswordGenerationUIData(
      gfx::RectF(100, 20), /*max_length=*/20, u"element",
      autofill::FieldRendererId(100),
      /*is_generation_element_password_type=*/true, base::i18n::TextDirection(),
      autofill::FormData(), /*input_field_empty=*/true);
}

class DropFullscreenDelegate : public content::WebContentsDelegate {
 public:
  explicit DropFullscreenDelegate(base::RepeatingClosure on_exit)
      : on_exit_(std::move(on_exit)) {}

  content::FullscreenState GetFullscreenState(
      const content::WebContents* web_contents) const override {
    content::FullscreenState state;
    state.target_mode = content::FullscreenMode::kContent;
    return state;
  }

  void ExitFullscreenModeForTab(content::WebContents* web_contents) override {
    if (on_exit_) {
      on_exit_.Run();
    }
  }

 private:
  base::RepeatingClosure on_exit_;
};

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD(void,
              GeneratedPasswordAccepted,
              (const std::u16string&),
              (override));
  MOCK_METHOD(void,
              GeneratedPasswordAccepted,
              (const autofill::FormData&,
               autofill::FieldRendererId,
               const std::u16string&),
              (override));
  MOCK_METHOD(void, GeneratedPasswordRejected, (), (override));
  MOCK_METHOD(PasswordGenerationFrameHelper*,
              GetPasswordGenerationHelper,
              (),
              (override));
  MOCK_METHOD(void,
              PreviewGenerationSuggestion,
              (const std::u16string& password),
              (override));
  MOCK_METHOD(void, ClearPreviewedForm, (), (override));
  MOCK_METHOD(void, FocusNextFieldAfterPasswords, (), (override));
};

class MockPasswordGenerationPopupView : public PasswordGenerationPopupView {
 public:
  MOCK_METHOD(bool, Show, (), (override));
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(void, UpdateState, (), (override));
  MOCK_METHOD(void, UpdateGeneratedPasswordValue, (), (override));
  MOCK_METHOD(bool, UpdateBoundsAndRedrawPopup, (), (override));
  MOCK_METHOD(void, ButtonSelectionUpdated, (), (override));
};

class PasswordGenerationPopupControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<MockPasswordManagerDriver> CreateDriver();

 protected:
  MockPasswordGenerationPopupView* popup_view() { return &view_; }
  MockPasswordManagerDriver& driver() { return *driver_; }
  base::WeakPtr<PasswordManagerDriver> weak_driver() {
    return driver_->AsWeakPtr();
  }
  content::WebContents* web_contents() { return web_contents_.get(); }
  PasswordGenerationUIData& ui_data() { return ui_data_; }

  base::WeakPtr<PasswordGenerationPopupControllerImpl> GetOrCreateController(
      base::WeakPtr<PasswordGenerationPopupControllerImpl> previous = nullptr) {
    return PasswordGenerationPopupControllerImpl::GetOrCreate(
        previous, ui_data().bounds, ui_data(), weak_driver(),
        /*observer=*/nullptr, web_contents(), main_rfh());
  }

  base::WeakPtr<PasswordGenerationPopupControllerImpl>
  CreateControllerWithView() {
    base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
        GetOrCreateController();
    controller->SetViewForTesting(popup_view());
    return controller;
  }

  base::WeakPtr<PasswordGenerationPopupControllerImpl> ShowOfferGenerationPopup(
      bool is_manually_triggered = false) {
    base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
        CreateControllerWithView();
    controller->GeneratePasswordValue(PasswordGenerationType::kAutomatic);
    controller->Show(
        PasswordGenerationPopupController::GenerationUIState::kOfferGeneration,
        is_manually_triggered);
    return controller;
  }

  bool SimulateKeyPress(
      base::WeakPtr<PasswordGenerationPopupControllerImpl> controller,
      int windows_key_code,
      int modifiers = blink::WebInputEvent::kNoModifiers) {
    input::NativeWebKeyboardEvent event(
        blink::WebInputEvent::Type::kRawKeyDown, modifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    event.windows_key_code = windows_key_code;
    return controller->HandleKeyPressEventForTesting(event);
  }

 private:
  StubPasswordManagerClient client_;
  std::unique_ptr<MockPasswordManagerDriver> driver_;
  std::unique_ptr<PasswordGenerationFrameHelper> pw_generation_helper_;
  std::unique_ptr<content::WebContents> web_contents_;
  PasswordGenerationUIData ui_data_;
  MockPasswordGenerationPopupView view_;
};

void PasswordGenerationPopupControllerImplTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  driver_ = CreateDriver();
  web_contents_ = CreateTestWebContents();
  ui_data_ = CreatePasswordGenerationUIData();

  // The password generation helper is needed in the offer generation state and
  // since the driver mock returns a raw pointer to it, we construct it first.
  pw_generation_helper_ =
      std::make_unique<PasswordGenerationFrameHelper>(&client_, driver_.get());
  ON_CALL(driver(), GetPasswordGenerationHelper)
      .WillByDefault(Return(pw_generation_helper_.get()));
  ON_CALL(*popup_view(), UpdateBoundsAndRedrawPopup)
      .WillByDefault(Return(true));
}

void PasswordGenerationPopupControllerImplTest::TearDown() {
  web_contents_.reset();
  ChromeRenderViewHostTestHarness::TearDown();
}

std::unique_ptr<MockPasswordManagerDriver>
PasswordGenerationPopupControllerImplTest::CreateDriver() {
  return std::make_unique<MockPasswordManagerDriver>();
}

}  // namespace

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateTheSame) {
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  EXPECT_EQ(controller1.get(), controller2.get());
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateDifferentBounds) {
  gfx::RectF rect(100, 20);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, rect, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  rect = gfx::RectF(200, 30);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, rect, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateDifferentTabs) {
  auto web_contents1 = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents1.get(), main_rfh());

  auto web_contents2 = CreateTestWebContents();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents2.get(), main_rfh());

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetOrCreateDifferentDrivers) {
  auto driver1 = CreateDriver();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(),
          driver1->AsWeakPtr(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  auto driver2 = CreateDriver();
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data().bounds, ui_data(), driver2->AsWeakPtr(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       GetOrCreateDifferentElements) {
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller1 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  ui_data().generation_element_id = autofill::FieldRendererId(200);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller2 =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          controller1, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  EXPECT_FALSE(controller1);
  EXPECT_TRUE(controller2);
}

TEST_F(PasswordGenerationPopupControllerImplTest, DestroyInPasswordAccepted) {
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  // Destroying the controller in GeneratedPasswordAccepted() should not cause a
  // crash.
  EXPECT_CALL(driver(),
              GeneratedPasswordAccepted(_, autofill::FieldRendererId(100), _))
      .WillOnce([controller](auto, auto, auto) {
        controller->Hide(autofill::SuggestionHidingReason::kViewDestroyed);
      });
  controller->PasswordAccepted();
}

// Tests that accepting the password doesn't crash if the driver is gone, e.g.
// because the frame was deleted since the popup was shown.
TEST_F(PasswordGenerationPopupControllerImplTest,
       PasswordAcceptedWithoutDriverDoesNotCrash) {
  std::unique_ptr<MockPasswordManagerDriver> driver = CreateDriver();
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(),
          driver->AsWeakPtr(), /*observer=*/nullptr, web_contents(),
          main_rfh());
  driver.reset();

  controller->PasswordAccepted();
  EXPECT_FALSE(controller);
}

TEST_F(PasswordGenerationPopupControllerImplTest, GetElementTextDirection) {
  ui_data().text_direction = base::i18n::TextDirection::RIGHT_TO_LEFT;
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  ASSERT_TRUE(controller);
  EXPECT_EQ(controller->GetElementTextDirection(),
            base::i18n::TextDirection::RIGHT_TO_LEFT);
}

TEST_F(PasswordGenerationPopupControllerImplTest, ClearsFormPreviewOnHide) {
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  EXPECT_CALL(driver(), ClearPreviewedForm());
  controller->Hide(autofill::SuggestionHidingReason::kViewDestroyed);
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       SuggestedTextDefaultPasswordLength) {
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());
  controller->SetViewForTesting(popup_view());

  controller->GeneratePasswordValue(PasswordGenerationType::kAutomatic);
  EXPECT_EQ(static_cast<PasswordGenerationPopupController*>(controller.get())
                ->SuggestedText(),
            l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_SUGGESTION_GPM));
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       SuggestedTextShorterPasswordLength) {
  // Limit the max length of the password.
  ui_data().max_length = 10;

  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());
  controller->SetViewForTesting(popup_view());

  controller->GeneratePasswordValue(PasswordGenerationType::kAutomatic);
  EXPECT_EQ(static_cast<PasswordGenerationPopupController*>(controller.get())
                ->SuggestedText(),
            l10n_util::GetStringUTF16(
                IDS_PASSWORD_GENERATION_SUGGESTION_GPM_WITHOUT_STRONG));
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       AdvancesFieldFocusOnUseStrongPassword) {
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  EXPECT_CALL(driver(),
              GeneratedPasswordAccepted(_, autofill::FieldRendererId(100), _));
  EXPECT_CALL(driver(), FocusNextFieldAfterPasswords);
  controller->PasswordAccepted();
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       InformsDriverAboutPasswordRejection) {
  base::WeakPtr<PasswordGenerationPopupController> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  EXPECT_CALL(driver(), GeneratedPasswordRejected());
  controller->PasswordRejected();
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       PreviewsGeneratedPasswordOnShowInNudgePassword) {
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  controller->SetViewForTesting(popup_view());
  // TODO(crbug.com/40267532): Rewrite controller_->Show() function to allow
  // testing expectations when the view doesn't exist.  SetViewForTesting
  // prevents that currently, hence the update view flow is being called.
  ON_CALL(*popup_view(), UpdateBoundsAndRedrawPopup)
      .WillByDefault(Return(true));

  // In the nudge password experiment suggestion is previewed on show.
  controller->GeneratePasswordValue(PasswordGenerationType::kAutomatic);
  EXPECT_CALL(driver(), PreviewGenerationSuggestion);
  controller->Show(
      PasswordGenerationPopupController::GenerationUIState::kOfferGeneration);
}

TEST_F(PasswordGenerationPopupControllerImplTest, ShowDropsFullscreen) {
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  controller->SetViewForTesting(popup_view());
  ON_CALL(*popup_view(), UpdateBoundsAndRedrawPopup)
      .WillByDefault(Return(true));

  bool exit_fullscreen_called = false;
  DropFullscreenDelegate delegate(
      base::BindLambdaForTesting([&]() { exit_fullscreen_called = true; }));
  web_contents()->SetDelegate(&delegate);

  controller->GeneratePasswordValue(PasswordGenerationType::kAutomatic);
  controller->Show(
      PasswordGenerationPopupController::GenerationUIState::kOfferGeneration);

  EXPECT_TRUE(exit_fullscreen_called);
  web_contents()->SetDelegate(nullptr);
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       DestroyedWhileExitingFullscreenDoesNotCrash) {
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      PasswordGenerationPopupControllerImpl::GetOrCreate(
          /*previous=*/nullptr, ui_data().bounds, ui_data(), weak_driver(),
          /*observer=*/nullptr, web_contents(), main_rfh());

  controller->SetViewForTesting(popup_view());

  DropFullscreenDelegate delegate(base::BindLambdaForTesting([&]() {
    if (controller) {
      static_cast<PasswordGenerationPopupController*>(controller.get())
          ->Hide(autofill::SuggestionHidingReason::kViewDestroyed);
    }
  }));
  web_contents()->SetDelegate(&delegate);

  controller->GeneratePasswordValue(PasswordGenerationType::kAutomatic);
  controller->Show(
      PasswordGenerationPopupController::GenerationUIState::kOfferGeneration);

  EXPECT_FALSE(controller);
  web_contents()->SetDelegate(nullptr);
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       UnenteredTabFallsThroughToBlink) {
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      CreateControllerWithView();
  // Prior to Show() or when not offering generation, IsSelectable() is false
  // and key presses must not be consumed.
  EXPECT_FALSE(SimulateKeyPress(controller, ui::VKEY_TAB));

  controller->GeneratePasswordValue(PasswordGenerationType::kAutomatic);
  controller->Show(
      PasswordGenerationPopupController::GenerationUIState::kOfferGeneration);

  EXPECT_FALSE(controller->accept_button_selected_for_testing());
  EXPECT_FALSE(controller->cancel_button_selected_for_testing());

  // When the popup is unentered, pressing Tab or Shift+Tab must return false
  // so Blink handles standard form field focus navigation without trapping
  // keyboard users inside the password field.
  EXPECT_FALSE(SimulateKeyPress(controller, ui::VKEY_TAB));
  EXPECT_FALSE(controller->accept_button_selected_for_testing());
  EXPECT_FALSE(controller->cancel_button_selected_for_testing());

  EXPECT_FALSE(SimulateKeyPress(controller, ui::VKEY_TAB,
                                blink::WebInputEvent::kShiftKey));
  EXPECT_FALSE(controller->accept_button_selected_for_testing());
  EXPECT_FALSE(controller->cancel_button_selected_for_testing());
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       DownArrowEntryAndIntraPopupTabCycling) {
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      ShowOfferGenerationPopup();

  // Down arrow from unentered state selects the Accept button first.
  EXPECT_TRUE(SimulateKeyPress(controller, ui::VKEY_DOWN));
  EXPECT_TRUE(controller->accept_button_selected_for_testing());
  EXPECT_FALSE(controller->cancel_button_selected_for_testing());

  // Once entered, Tab cycles to the Cancel button.
  EXPECT_TRUE(SimulateKeyPress(controller, ui::VKEY_TAB));
  EXPECT_FALSE(controller->accept_button_selected_for_testing());
  EXPECT_TRUE(controller->cancel_button_selected_for_testing());

  // Tab on the Cancel button wraps around to the Accept button.
  EXPECT_TRUE(SimulateKeyPress(controller, ui::VKEY_TAB));
  EXPECT_TRUE(controller->accept_button_selected_for_testing());
  EXPECT_FALSE(controller->cancel_button_selected_for_testing());

  // Shift+Tab on the Accept button wraps around to the Cancel button.
  EXPECT_TRUE(SimulateKeyPress(controller, ui::VKEY_TAB,
                               blink::WebInputEvent::kShiftKey));
  EXPECT_FALSE(controller->accept_button_selected_for_testing());
  EXPECT_TRUE(controller->cancel_button_selected_for_testing());

  // Shift+Tab on the Cancel button cycles back to the Accept button.
  EXPECT_TRUE(SimulateKeyPress(controller, ui::VKEY_TAB,
                               blink::WebInputEvent::kShiftKey));
  EXPECT_TRUE(controller->accept_button_selected_for_testing());
  EXPECT_FALSE(controller->cancel_button_selected_for_testing());
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       UpArrowEntrySelectsCancelButton) {
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      ShowOfferGenerationPopup();

  // Up arrow from unentered state selects the Cancel button.
  EXPECT_TRUE(SimulateKeyPress(controller, ui::VKEY_UP));
  EXPECT_FALSE(controller->accept_button_selected_for_testing());
  EXPECT_TRUE(controller->cancel_button_selected_for_testing());

  // Tab then cycles to the Accept button.
  EXPECT_TRUE(SimulateKeyPress(controller, ui::VKEY_TAB));
  EXPECT_TRUE(controller->accept_button_selected_for_testing());
  EXPECT_FALSE(controller->cancel_button_selected_for_testing());
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       ReturnKeyOnAcceptButtonAcceptsPasswordAndAdvancesFocus) {
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      ShowOfferGenerationPopup();

  // Enter popup via Down arrow to select Accept button.
  EXPECT_TRUE(SimulateKeyPress(controller, ui::VKEY_DOWN));
  EXPECT_TRUE(controller->accept_button_selected_for_testing());

  EXPECT_CALL(driver(),
              GeneratedPasswordAccepted(_, autofill::FieldRendererId(100), _));
  EXPECT_CALL(driver(), FocusNextFieldAfterPasswords());
  EXPECT_TRUE(SimulateKeyPress(controller, ui::VKEY_RETURN));

  // Accepting the generated password executes HideImpl(), which destroys the
  // controller and closes the popup dialog.
  EXPECT_FALSE(controller);
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       ManuallyTriggeredInitialSelection) {
  // When generation is triggered manually (e.g. via the context menu "Suggest
  // strong password"), the popup should immediately enter the selected state
  // so the user can interact directly without needing arrow key navigation
  // first.
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      ShowOfferGenerationPopup(/*is_manually_triggered=*/true);

  EXPECT_TRUE(controller->accept_button_selected_for_testing());
  EXPECT_FALSE(controller->cancel_button_selected_for_testing());
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       ModifiedKeyPressesAreIgnored) {
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      ShowOfferGenerationPopup();

  // Modified arrow and escape keys must not enter or dismiss the popup.
  EXPECT_FALSE(SimulateKeyPress(controller, ui::VKEY_DOWN,
                                blink::WebInputEvent::kShiftKey));
  EXPECT_FALSE(SimulateKeyPress(controller, ui::VKEY_DOWN,
                                blink::WebInputEvent::kControlKey));
  EXPECT_FALSE(
      SimulateKeyPress(controller, ui::VKEY_UP, blink::WebInputEvent::kAltKey));
  EXPECT_FALSE(SimulateKeyPress(controller, ui::VKEY_ESCAPE,
                                blink::WebInputEvent::kShiftKey));
  ASSERT_TRUE(controller);
  EXPECT_FALSE(controller->accept_button_selected_for_testing());
  EXPECT_FALSE(controller->cancel_button_selected_for_testing());

  // Enter the popup so the Accept button is selected.
  EXPECT_TRUE(SimulateKeyPress(controller, ui::VKEY_DOWN));
  EXPECT_TRUE(controller->accept_button_selected_for_testing());

  EXPECT_CALL(driver(), GeneratedPasswordAccepted(_, _, _)).Times(0);
  EXPECT_CALL(driver(), GeneratedPasswordRejected()).Times(0);

  // Modified Tab, arrow, Return, and Escape keys must not be consumed or
  // change the selection state.
  EXPECT_FALSE(SimulateKeyPress(controller, ui::VKEY_TAB,
                                blink::WebInputEvent::kControlKey));
  EXPECT_FALSE(SimulateKeyPress(controller, ui::VKEY_TAB,
                                blink::WebInputEvent::kAltKey));
  EXPECT_FALSE(SimulateKeyPress(controller, ui::VKEY_TAB,
                                blink::WebInputEvent::kMetaKey));
  EXPECT_FALSE(SimulateKeyPress(controller, ui::VKEY_DOWN,
                                blink::WebInputEvent::kShiftKey));
  EXPECT_FALSE(SimulateKeyPress(controller, ui::VKEY_DOWN,
                                blink::WebInputEvent::kControlKey));
  EXPECT_FALSE(SimulateKeyPress(controller, ui::VKEY_RETURN,
                                blink::WebInputEvent::kControlKey));
  EXPECT_FALSE(SimulateKeyPress(controller, ui::VKEY_ESCAPE,
                                blink::WebInputEvent::kShiftKey));

  ASSERT_TRUE(controller);
  EXPECT_TRUE(controller->accept_button_selected_for_testing());
  EXPECT_FALSE(controller->cancel_button_selected_for_testing());
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       TransitionToEditGeneratedPasswordResetsSelection) {
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      ShowOfferGenerationPopup();

  // Select the Cancel button in kOfferGeneration, then transition to
  // kEditGeneratedPassword.
  EXPECT_TRUE(SimulateKeyPress(controller, ui::VKEY_UP));
  EXPECT_TRUE(controller->cancel_button_selected_for_testing());

  controller->Show(PasswordGenerationPopupController::GenerationUIState::
                       kEditGeneratedPassword);
  EXPECT_FALSE(controller->accept_button_selected_for_testing());
  EXPECT_FALSE(controller->cancel_button_selected_for_testing());

  // Pressing Return in kEditGeneratedPassword must not crash or reject.
  EXPECT_CALL(driver(), GeneratedPasswordRejected()).Times(0);
  EXPECT_FALSE(SimulateKeyPress(controller, ui::VKEY_RETURN));
  ASSERT_TRUE(controller);

  // Transition back to kOfferGeneration, select the Accept button, and
  // transition to kEditGeneratedPassword again.
  controller->Show(
      PasswordGenerationPopupController::GenerationUIState::kOfferGeneration);
  EXPECT_TRUE(SimulateKeyPress(controller, ui::VKEY_DOWN));
  EXPECT_TRUE(controller->accept_button_selected_for_testing());

  controller->Show(PasswordGenerationPopupController::GenerationUIState::
                       kEditGeneratedPassword);
  EXPECT_FALSE(controller->accept_button_selected_for_testing());
  EXPECT_FALSE(controller->cancel_button_selected_for_testing());

  EXPECT_CALL(driver(), GeneratedPasswordAccepted(_, _, _)).Times(0);
  EXPECT_FALSE(SimulateKeyPress(controller, ui::VKEY_RETURN));
  EXPECT_TRUE(controller);
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       ManuallyTriggeredReShowUpdatesButtonSelection) {
  EXPECT_CALL(*popup_view(), UpdateState());
  EXPECT_CALL(*popup_view(), ButtonSelectionUpdated());
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      ShowOfferGenerationPopup(/*is_manually_triggered=*/true);
  EXPECT_TRUE(controller->accept_button_selected_for_testing());
  testing::Mock::VerifyAndClearExpectations(popup_view());

  // Calling Show(kOfferGeneration, /*is_manually_triggered=*/true) again when
  // the view already exists (which recreates the buttons via UpdateState())
  // must reset and re-apply the selection so ButtonSelectionUpdated() is
  // notified.
  ON_CALL(*popup_view(), UpdateBoundsAndRedrawPopup)
      .WillByDefault(Return(true));
  EXPECT_CALL(*popup_view(), UpdateState());
  EXPECT_CALL(*popup_view(), ButtonSelectionUpdated());
  controller->Show(
      PasswordGenerationPopupController::GenerationUIState::kOfferGeneration,
      /*is_manually_triggered=*/true);
  EXPECT_TRUE(controller->accept_button_selected_for_testing());
  EXPECT_FALSE(controller->cancel_button_selected_for_testing());
}

TEST_F(PasswordGenerationPopupControllerImplTest,
       AutomaticallyTriggeredDoesNotAutoSelectEvenWithScreenReaderActive) {
  const ui::AssistiveTech previous_tech =
      ui::AXPlatform::GetInstance().active_assistive_tech();
  ui::AXPlatform::GetInstance().NotifyAssistiveTechChanged(
      ui::AssistiveTech::kGenericScreenReader);
  base::ScopedClosureRunner restore_assistive_tech(base::BindOnce(
      [](ui::AssistiveTech tech) {
        ui::AXPlatform::GetInstance().NotifyAssistiveTechChanged(tech);
      },
      previous_tech));
  ASSERT_TRUE(ui::AXPlatform::GetInstance().IsScreenReaderActive());

  // An automatically triggered popup must not auto-select the accept button on
  // show, even when a screen reader is active, so that focus stays in the
  // password input until the user explicitly navigates into the popup.
  EXPECT_CALL(*popup_view(), ButtonSelectionUpdated()).Times(0);
  base::WeakPtr<PasswordGenerationPopupControllerImpl> controller =
      ShowOfferGenerationPopup(/*is_manually_triggered=*/false);
  EXPECT_FALSE(controller->accept_button_selected_for_testing());
  EXPECT_FALSE(controller->cancel_button_selected_for_testing());
}

}  // namespace password_manager
