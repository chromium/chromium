// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_TEST_BASE_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_TEST_BASE_H_

#include <concepts>
#include <memory>
#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
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
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/ui/autofill_popup_delegate.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/autofill/mock_manual_filling_view.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller_impl.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_address_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_credit_card_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_password_accessory_controller.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

class AutofillPopupControllerForPopupTest;

namespace internal {

// Satisfied if `D` is derived from either `B` or a mock wrapper around `B`.
template <typename D, typename B>
concept DerivedFromClassOrMock =
    std::derived_from<D, B> || std::derived_from<D, ::testing::NaggyMock<B>> ||
    std::derived_from<D, ::testing::NiceMock<B>> ||
    std::derived_from<D, ::testing::StrictMock<B>>;

template <typename Controller, typename Driver>
concept ControllerAndDriver =
    DerivedFromClassOrMock<Controller, AutofillPopupControllerForPopupTest> &&
    DerivedFromClassOrMock<Driver, ContentAutofillDriver>;

}  // namespace internal

// This text fixture is intended for unit tests of the Autofill popup
// controller, which controls the Autofill popup on Desktop and the Keyboard
// Accessory on Clank. It has two template parameters that allow customizing the
// test fixture's behavior:
// - The class of the `AutofillPopupController` to test. The use of this
//   parameter is to be able to test different implementations of the
//   `AutofillPopupController` interface.
// - The class of the `AutofillDriver` to inject, used, e.g., in a11y-specific
//   tests.
//
// The main reason for the complexity of the test fixture is that there is
// little value in testing an `AutofillPopupController` just by itself: Most of
// its behavior depends on interactions with the `WebContents`, the
// `AutofillClient`, or the `AutofillPopupView`. This test fixture sets these up
// in a way that allows for controller testing.
//
// Once setup, the test fixture should allow writing popup controller unit tests
// that closely mirror the production setup. Example:
//
// using SampleTest = AutofillPopupControllerTestBase<>;
//
// TEST_F(SampleTest, AcceptSuggestionWorksAfter500Ms) {
//   ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
//   EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion);
//   task_environment()->FastForwardBy(base::Milliseconds(500));
//   client().popup_controller(manager()).AcceptSuggestion(/*index=*/0);
// }
template <typename Controller =
              ::testing::NiceMock<AutofillPopupControllerForPopupTest>,
          typename Driver = ContentAutofillDriver>
  requires(internal::ControllerAndDriver<Controller, Driver>)
class AutofillPopupControllerTestBase : public ChromeRenderViewHostTestHarness {
 public:
  AutofillPopupControllerTestBase()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AutofillPopupControllerTestBase() override = default;

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
        mock_address_controller_.AsWeakPtr(), mock_cc_controller_.AsWeakPtr(),
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

  class TestManager;
  class TestClient;

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  TestClient& client() { return *autofill_client_injector_[web_contents()]; }

  Driver& driver(content::RenderFrameHost* rfh = nullptr) {
    return *autofill_driver_injector_[rfh ? rfh : main_frame()];
  }

  TestManager& manager(content::RenderFrameHost* rfh = nullptr) {
    return *autofill_manager_injector_[rfh ? rfh : main_frame()];
  }

  TestPersonalDataManager& personal_data() {
    return static_cast<TestPersonalDataManager&>(
        *PersonalDataManagerFactory::GetForProfile(profile()));
  }

  // Shows empty suggestions with the popup_item_id ids passed as
  // `popup_item_ids`.
  void ShowSuggestions(
      TestManager& manager,
      const std::vector<PopupItemId>& popup_item_ids,
      AutofillSuggestionTriggerSource trigger_source =
          AutofillSuggestionTriggerSource::kFormControlElementClicked) {
    std::vector<Suggestion> suggestions;
    suggestions.reserve(popup_item_ids.size());
    for (PopupItemId popup_item_id : popup_item_ids) {
      suggestions.emplace_back(u"", popup_item_id);
    }
    ShowSuggestions(manager, std::move(suggestions), trigger_source);
  }

  void ShowSuggestions(
      TestManager& manager,
      std::vector<Suggestion> suggestions,
      AutofillSuggestionTriggerSource trigger_source =
          AutofillSuggestionTriggerSource::kFormControlElementClicked) {
    FocusWebContentsOnFrame(
        static_cast<ContentAutofillDriver&>(manager.driver())
            .render_frame_host());
    client().popup_controller(manager).Show(std::move(suggestions),
                                            trigger_source,
                                            AutoselectFirstSuggestion(false));
  }

  content::NativeWebKeyboardEvent CreateKeyPressEvent(int windows_key_code) {
    content::NativeWebKeyboardEvent event(
        blink::WebInputEvent::Type::kRawKeyDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    event.windows_key_code = windows_key_code;
    return event;
  }

 private:
  ::autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;

  TestAutofillClientInjector<TestClient> autofill_client_injector_;
  TestAutofillDriverInjector<Driver> autofill_driver_injector_;
  TestAutofillManagerInjector<TestManager> autofill_manager_injector_;

#if BUILDFLAG(IS_ANDROID)
  ::testing::NiceMock<MockPasswordAccessoryController> mock_pwd_controller_;
  ::testing::NiceMock<MockAddressAccessoryController> mock_address_controller_;
  ::testing::NiceMock<MockCreditCardAccessoryController> mock_cc_controller_;
#endif  // BUILDFLAG(IS_ANDROID)
};

// Below are test versions of `AutofillClient`, `BrowserAutofillManager`,
// `AutofillExternalDelegate` and `AutofillPopupController` that are used in the
// fixture above.

class AutofillExternalDelegateForPopupTest : public AutofillExternalDelegate {
 public:
  explicit AutofillExternalDelegateForPopupTest(
      BrowserAutofillManager* autofill_manager);
  ~AutofillExternalDelegateForPopupTest() override;

  void DidSelectSuggestion(const Suggestion& suggestion) override {}

  MOCK_METHOD(void, ClearPreviewedForm, (), (override));
  MOCK_METHOD(void, OnPopupShown, (), (override));
  MOCK_METHOD(void, OnPopupHidden, (), (override));
  MOCK_METHOD(void,
              DidAcceptSuggestion,
              (const Suggestion&,
               const AutofillPopupDelegate::SuggestionPosition&),
              (override));
  MOCK_METHOD(void,
              DidPerformButtonActionForSuggestion,
              (const Suggestion&),
              (override));
  MOCK_METHOD(bool, RemoveSuggestion, (const Suggestion&), (override));
};

// A `BrowserAutofillManager` with a modified `AutofillExternalDelegate` that
// allows verifying interactions with the popup.
template <typename Controller, typename Driver>
  requires(internal::ControllerAndDriver<Controller, Driver>)
class AutofillPopupControllerTestBase<Controller, Driver>::TestManager
    : public BrowserAutofillManager {
 public:
  explicit TestManager(AutofillDriver* driver)
      : BrowserAutofillManager(driver, "en-US") {
    test_api(*this).SetExternalDelegate(
        std::make_unique<
            ::testing::NiceMock<AutofillExternalDelegateForPopupTest>>(this));
  }
  TestManager(TestManager&) = delete;
  TestManager& operator=(TestManager&) = delete;
  ~TestManager() override = default;

  AutofillExternalDelegateForPopupTest& external_delegate() {
    return static_cast<AutofillExternalDelegateForPopupTest&>(
        *test_api(*this).external_delegate());
  }
};

// A modified `TestContentAutofillClient` that simulates the production behavior
// of the controller lifetime.
template <typename Controller, typename Driver>
  requires(internal::ControllerAndDriver<Controller, Driver>)
class AutofillPopupControllerTestBase<Controller, Driver>::TestClient
    : public TestContentAutofillClient {
 public:
  explicit TestClient(content::WebContents* web_contents)
      : TestContentAutofillClient(web_contents) {
    ON_CALL(popup_view(), CreateSubPopupView)
        .WillByDefault(::testing::Return(sub_popup_view().GetWeakPtr()));
  }

  ~TestClient() override { DoHide(); }

  // Returns the current controller. Controllers are specific to the `manager`'s
  // AutofillExternalDelegate. Therefore, when there are two consecutive
  // `popup_controller(x)` and `popup_controller(y)`, the second call hides the
  // old and creates new controller iff `x` and `y` are distinct.
  Controller& popup_controller(TestManager& manager) {
    if (manager_of_last_controller_.get() != &manager) {
      DoHide();
      CHECK(!popup_controller_);
    }
    if (!popup_controller_) {
      popup_controller_ =
          (new Controller(manager.external_delegate().GetWeakPtrForTest(),
                          &GetWebContents(), gfx::RectF(),
                          show_pwd_migration_warning_callback_.Get()))
              ->GetWeakPtr();
      cast_popup_controller().SetViewForTesting(popup_view_->GetWeakPtr());
      manager_of_last_controller_ = manager.GetWeakPtr();
      ON_CALL(cast_popup_controller(), Hide)
          .WillByDefault([this](PopupHidingReason reason) { DoHide(reason); });
    }
    return cast_popup_controller();
  }

  MockAutofillPopupView& popup_view() { return *popup_view_; }

  MockAutofillPopupView& sub_popup_view() { return *sub_popup_view_; }

#if BUILDFLAG(IS_ANDROID)
  base::MockCallback<base::RepeatingCallback<
      void(gfx::NativeWindow,
           Profile*,
           password_manager::metrics_util::PasswordMigrationWarningTriggers)>>&
  show_pwd_migration_warning_callback() {
    return show_pwd_migration_warning_callback_;
  }
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  void DoHide(PopupHidingReason reason) {
    if (popup_controller_) {
      cast_popup_controller().DoHide(reason);
    }
  }

  void DoHide() {
    if (popup_controller_) {
      cast_popup_controller().DoHide();
    }
  }

  Controller& cast_popup_controller() {
    return static_cast<Controller&>(*popup_controller_);
  }

  base::WeakPtr<AutofillPopupController> popup_controller_;
  base::WeakPtr<AutofillManager> manager_of_last_controller_;

  std::unique_ptr<MockAutofillPopupView> popup_view_ =
      std::make_unique<::testing::NiceMock<MockAutofillPopupView>>();
  std::unique_ptr<MockAutofillPopupView> sub_popup_view_ =
      std::make_unique<::testing::NiceMock<MockAutofillPopupView>>();
  base::MockCallback<base::RepeatingCallback<void(
      gfx::NativeWindow,
      Profile*,
      password_manager::metrics_util::PasswordMigrationWarningTriggers)>>
      show_pwd_migration_warning_callback_;
};

class AutofillPopupControllerForPopupTest : public AutofillPopupControllerImpl {
 public:
  AutofillPopupControllerForPopupTest(
      base::WeakPtr<AutofillExternalDelegate> external_delegate,
      content::WebContents* web_contents,
      const gfx::RectF& element_bounds,
      base::RepeatingCallback<void(
          gfx::NativeWindow,
          Profile*,
          password_manager::metrics_util::PasswordMigrationWarningTriggers)>
          show_pwd_migration_warning_callback,
      std::optional<base::WeakPtr<ExpandablePopupParentControllerImpl>> parent =
          std::nullopt);
  ~AutofillPopupControllerForPopupTest() override;

  // Making protected functions public for testing
  using AutofillPopupControllerImpl::AcceptSuggestion;
  using AutofillPopupControllerImpl::element_bounds;
  using AutofillPopupControllerImpl::FireControlsChangedEvent;
  using AutofillPopupControllerImpl::GetLineCount;
  using AutofillPopupControllerImpl::GetSuggestionAt;
  using AutofillPopupControllerImpl::GetSuggestionLabelsAt;
  using AutofillPopupControllerImpl::GetSuggestionMainTextAt;
  using AutofillPopupControllerImpl::GetWeakPtr;
  using AutofillPopupControllerImpl::PerformButtonActionForSuggestion;
  using AutofillPopupControllerImpl::RemoveSuggestion;
  using AutofillPopupControllerImpl::SelectSuggestion;
  MOCK_METHOD(void, Hide, (PopupHidingReason reason), (override));

  void DoHide(PopupHidingReason reason = PopupHidingReason::kTabGone) {
    AutofillPopupControllerImpl::Hide(reason);
  }
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_TEST_BASE_H_
