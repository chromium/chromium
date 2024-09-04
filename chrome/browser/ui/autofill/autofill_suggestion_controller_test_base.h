// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_SUGGESTION_CONTROLLER_TEST_BASE_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_SUGGESTION_CONTROLLER_TEST_BASE_H_

#include <concepts>
#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/browser/ui/suggestion_button_action.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/autofill/mock_manual_filling_view.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller_impl.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_address_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_password_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_payment_method_accessory_controller.h"
#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_controller_impl.h"
#else
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

class AutofillExternalDelegateForPopupTest;
class AutofillSuggestionControllerForTest;

// A `BrowserAutofillManager` with a modified `AutofillExternalDelegate` that
// allows verifying interactions with the popup.
class BrowserAutofillManagerForPopupTest : public BrowserAutofillManager {
 public:
  explicit BrowserAutofillManagerForPopupTest(AutofillDriver* driver);
  ~BrowserAutofillManagerForPopupTest() override;

  AutofillExternalDelegateForPopupTest& external_delegate();
};

// This text fixture is intended for unit tests of the Autofill popup
// controller, which controls the Autofill popup on Desktop and the Keyboard
// Accessory on Clank. It has two template parameters that allow customizing the
// test fixture's behavior:
// - The class of the `AutofillSuggestionController` to test. The use of this
//   parameter is to be able to test different implementations of the
//   `AutofillSuggestionController` interface.
// - The class of the `AutofillDriver` to inject, used, e.g., in a11y-specific
//   tests.
//
// The main reason for the complexity of the test fixture is that there is
// little value in testing an `AutofillSuggestionController` just by itself:
// Most of its behavior depends on interactions with the `WebContents`, the
// `AutofillClient`, or the `AutofillPopupView`. This test fixture sets these up
// in a way that allows for controller testing.
//
// Once setup, the test fixture should allow writing suggestion controller unit
// tests (on both Desktop and Android) that closely mirror the production setup.
// Example for Desktop:
//
// using SampleTest = AutofillSuggestionControllerTestBase<
//     TestAutofillPopupControllerAutofillClient<>>;
//
// TEST_F(SampleTest, AcceptSuggestionWorksAfter500Ms) {
//   ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
//   EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion);
//   task_environment()->FastForwardBy(base::Milliseconds(500));
//   client().popup_controller(manager()).AcceptSuggestion(/*index=*/0);
// }
//
// The same test can be run on for the Keyboard Accessory on Android by simply
// changing the test fixture template parameter:
//
// using SampleTest = AutofillSuggestionControllerTestBase<
//     TestAutofillKeyboardAccessoryControllerAutofillClient<>>;
template <typename Client, typename Driver = ContentAutofillDriver>
  requires(std::derived_from<Client, ContentAutofillClient> &&
           std::derived_from<Driver, ContentAutofillDriver>)
class AutofillSuggestionControllerTestBase
    : public ChromeRenderViewHostTestHarness {
 public:
  AutofillSuggestionControllerTestBase()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AutofillSuggestionControllerTestBase() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<TestPersonalDataManager>();
        }));
    NavigateAndCommit(GURL("https://foo.com/"));
    FocusWebContentsOnMainFrame();
    ASSERT_TRUE(web_contents()->GetFocusedFrame());

#if BUILDFLAG(IS_ANDROID)
    ManualFillingControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_pwd_controller_.AsWeakPtr(),
        mock_address_controller_.AsWeakPtr(),
        mock_payment_method_controller_.AsWeakPtr(),
        std::make_unique<::testing::NiceMock<MockManualFillingView>>());
#endif  // BUILDFLAG(IS_ANDROID)
  }

  void TearDown() override {
    // Wait for the pending deletion of the controllers. Otherwise, the
    // controllers are destroyed after the WebContents, and each of them
    // receives a final Hide() call for which we'd need to add explicit
    // expectations.
    task_environment()->RunUntilIdle();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  using Manager = BrowserAutofillManagerForPopupTest;

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  Client& client() { return *autofill_client_injector_[web_contents()]; }

  Driver& driver(content::RenderFrameHost* rfh = nullptr) {
    return *autofill_driver_injector_[rfh ? rfh : main_frame()];
  }

  Manager& manager(content::RenderFrameHost* rfh = nullptr) {
    return *autofill_manager_injector_[rfh ? rfh : main_frame()];
  }

  TestPersonalDataManager& personal_data() {
    return static_cast<TestPersonalDataManager&>(
        *PersonalDataManagerFactory::GetForBrowserContext(profile()));
  }

  // Shows empty suggestions with the type ids passed as
  // `types`.
  void ShowSuggestions(
      Manager& manager,
      const std::vector<SuggestionType>& types,
      AutofillSuggestionTriggerSource trigger_source =
          AutofillSuggestionTriggerSource::kFormControlElementClicked) {
    std::vector<Suggestion> suggestions;
    suggestions.reserve(types.size());
    for (SuggestionType type : types) {
      suggestions.emplace_back(u"", type);
    }
    ShowSuggestions(manager, std::move(suggestions), trigger_source);
  }

  void ShowSuggestions(
      Manager& manager,
      std::vector<Suggestion> suggestions,
      AutofillSuggestionTriggerSource trigger_source =
          AutofillSuggestionTriggerSource::kFormControlElementClicked) {
    FocusWebContentsOnFrame(
        static_cast<ContentAutofillDriver&>(manager.driver())
            .render_frame_host());
    client().popup_controller(manager).Show(
        AutofillSuggestionController::GenerateSuggestionUiSessionId(),
        std::move(suggestions), trigger_source,
        AutoselectFirstSuggestion(false));
  }

  input::NativeWebKeyboardEvent CreateKeyPressEvent(int windows_key_code) {
    input::NativeWebKeyboardEvent event(
        blink::WebInputEvent::Type::kRawKeyDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    event.windows_key_code = windows_key_code;
    return event;
  }

 private:
  ::autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;

  TestAutofillClientInjector<Client> autofill_client_injector_;
  TestAutofillDriverInjector<Driver> autofill_driver_injector_;
  TestAutofillManagerInjector<Manager> autofill_manager_injector_;

#if BUILDFLAG(IS_ANDROID)
  ::testing::NiceMock<MockPasswordAccessoryController> mock_pwd_controller_;
  ::testing::NiceMock<MockAddressAccessoryController> mock_address_controller_;
  ::testing::NiceMock<MockPaymentMethodAccessoryController>
      mock_payment_method_controller_;
#endif  // BUILDFLAG(IS_ANDROID)
};

// Below are test versions of `AutofillClient`, `BrowserAutofillManager`,
// `AutofillExternalDelegate` and `AutofillSuggestionController` that are used in the
// fixture above.

class AutofillExternalDelegateForPopupTest : public AutofillExternalDelegate {
 public:
  explicit AutofillExternalDelegateForPopupTest(
      BrowserAutofillManager* autofill_manager);
  ~AutofillExternalDelegateForPopupTest() override;

  MOCK_METHOD(void, ClearPreviewedForm, (), (override));
  MOCK_METHOD(void,
              OnSuggestionsShown,
              (base::span<const Suggestion>),
              (override));
  MOCK_METHOD(void, OnSuggestionsHidden, (), (override));
  MOCK_METHOD(void, DidSelectSuggestion, (const Suggestion&), (override));
  MOCK_METHOD(void,
              DidAcceptSuggestion,
              (const Suggestion&,
               const AutofillSuggestionDelegate::SuggestionMetadata&),
              (override));
  MOCK_METHOD(void,
              DidPerformButtonActionForSuggestion,
              (const Suggestion&, const SuggestionButtonAction&),
              (override));
  MOCK_METHOD(bool, RemoveSuggestion, (const Suggestion&), (override));
};

using AutofillSuggestionControllerForTestBase =
#if BUILDFLAG(IS_ANDROID)
    AutofillKeyboardAccessoryControllerImpl;
#else
    AutofillPopupControllerImpl;
#endif

class AutofillSuggestionControllerForTest
    : public AutofillSuggestionControllerForTestBase {
 public:
  AutofillSuggestionControllerForTest(
      base::WeakPtr<AutofillExternalDelegate> external_delegate,
      content::WebContents* web_contents,
      const gfx::RectF& element_bounds
#if BUILDFLAG(IS_ANDROID)
      ,
      ShowPasswordMigrationWarningCallback show_pwd_migration_warning_callback
#endif
  );
  ~AutofillSuggestionControllerForTest() override;

  // Making protected functions public for testing
  using AutofillSuggestionControllerForTestBase::element_bounds;
  MOCK_METHOD(void, Hide, (SuggestionHidingReason reason), (override));

  void DoHide(
      SuggestionHidingReason reason = SuggestionHidingReason::kTabGone) {
    AutofillSuggestionControllerForTestBase::Hide(reason);
  }
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_SUGGESTION_CONTROLLER_TEST_BASE_H_
