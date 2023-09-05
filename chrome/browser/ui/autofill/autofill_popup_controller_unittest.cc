// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_driver_router.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_active_popup.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/text_utils.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "content/public/test/scoped_accessibility_mode_override.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/manual_filling_controller_impl.h"
#include "chrome/browser/autofill/mock_address_accessory_controller.h"
#include "chrome/browser/autofill/mock_credit_card_accessory_controller.h"
#include "chrome/browser/autofill/mock_manual_filling_view.h"
#include "chrome/browser/autofill/mock_password_accessory_controller.h"
#endif

using base::ASCIIToUTF16;
using base::WeakPtr;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace autofill {
namespace {

class MockAutofillDriver : public ContentAutofillDriver {
 public:
  MockAutofillDriver(content::RenderFrameHost* rfh,
                     ContentAutofillDriverFactory* factory)
      : ContentAutofillDriver(rfh, factory) {}

  MockAutofillDriver(MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(MockAutofillDriver&) = delete;

  ~MockAutofillDriver() override = default;
  MOCK_METHOD(ui::AXTreeID, GetAxTreeId, (), (const override));
};

class MockBrowserAutofillManager : public BrowserAutofillManager {
 public:
  MockBrowserAutofillManager(AutofillDriver* driver,
                             ContentAutofillClient* client)
      : BrowserAutofillManager(driver, client, "en-US") {}
  MockBrowserAutofillManager(MockBrowserAutofillManager&) = delete;
  MockBrowserAutofillManager& operator=(MockBrowserAutofillManager&) = delete;
  ~MockBrowserAutofillManager() override = default;
};

class MockAutofillExternalDelegate : public AutofillExternalDelegate {
 public:
  explicit MockAutofillExternalDelegate(
      BrowserAutofillManager* autofill_manager)
      : AutofillExternalDelegate(autofill_manager) {}
  ~MockAutofillExternalDelegate() override = default;

  void DidSelectSuggestion(
      const Suggestion& suggestion,
      AutofillSuggestionTriggerSource trigger_source) override {}
  bool RemoveSuggestion(const std::u16string& value,
                        PopupItemId popup_item_id,
                        Suggestion::BackendId backend_id) override {
    return true;
  }

  MOCK_METHOD(void, ClearPreviewedForm, (), (override));
  MOCK_METHOD(void,
              DidAcceptSuggestion,
              (const Suggestion&, int, AutofillSuggestionTriggerSource),
              (override));
};

class MockAutofillPopupView : public AutofillPopupView {
 public:
  MockAutofillPopupView() = default;
  MockAutofillPopupView(MockAutofillPopupView&) = delete;
  MockAutofillPopupView& operator=(MockAutofillPopupView&) = delete;
  ~MockAutofillPopupView() override = default;

  MOCK_METHOD(bool, Show, (AutoselectFirstSuggestion), (override));
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(bool,
              HandleKeyPressEvent,
              (const content::NativeWebKeyboardEvent&),
              (override));
  MOCK_METHOD(void, OnSuggestionsChanged, (), (override));
  MOCK_METHOD(bool, OverlapsWithPictureInPictureWindow, (), (const override));
  MOCK_METHOD(absl::optional<int32_t>, GetAxUniqueId, (), (override));
  MOCK_METHOD(void, AxAnnounce, (const std::u16string&), (override));
  MOCK_METHOD(base::WeakPtr<AutofillPopupView>,
              CreateSubPopupView,
              (base::WeakPtr<AutofillPopupController>),
              (override));

  base::WeakPtr<AutofillPopupView> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<AutofillPopupView> weak_ptr_factory_{this};
};

class TestAutofillPopupController : public AutofillPopupControllerImpl {
 public:
  TestAutofillPopupController(
      base::WeakPtr<AutofillExternalDelegate> external_delegate,
      content::WebContents* web_contents,
      const gfx::RectF& element_bounds,
      base::RepeatingCallback<void(
          gfx::NativeWindow,
          Profile*,
          password_manager::metrics_util::PasswordMigrationWarningTriggers)>
          show_pwd_migration_warning_callback,
      absl::optional<base::WeakPtr<ExpandablePopupParentControllerImpl>>
          parent = absl::nullopt)
      : AutofillPopupControllerImpl(
            external_delegate,
            web_contents,
            nullptr,
            element_bounds,
            base::i18n::UNKNOWN_DIRECTION,
            std::move(show_pwd_migration_warning_callback),
            parent) {}
  ~TestAutofillPopupController() override = default;

  // Making protected functions public for testing
  using AutofillPopupControllerImpl::AcceptSuggestion;
  using AutofillPopupControllerImpl::AcceptSuggestionWithoutThreshold;
  using AutofillPopupControllerImpl::element_bounds;
  using AutofillPopupControllerImpl::FireControlsChangedEvent;
  using AutofillPopupControllerImpl::GetLineCount;
  using AutofillPopupControllerImpl::GetRootAXPlatformNodeForWebContents;
  using AutofillPopupControllerImpl::GetSuggestionAt;
  using AutofillPopupControllerImpl::GetSuggestionLabelsAt;
  using AutofillPopupControllerImpl::GetSuggestionMainTextAt;
  using AutofillPopupControllerImpl::GetWeakPtr;
  using AutofillPopupControllerImpl::RemoveSuggestion;
  using AutofillPopupControllerImpl::SelectSuggestion;
  MOCK_METHOD(void, OnSuggestionsChanged, (), (override));
  MOCK_METHOD(void, Hide, (PopupHidingReason reason), (override));
  MOCK_METHOD(ui::AXPlatformNode*,
              GetRootAXPlatformNodeForWebContents,
              (),
              (override));

  void DoHide() { DoHide(PopupHidingReason::kTabGone); }

  void DoHide(PopupHidingReason reason) {
    AutofillPopupControllerImpl::Hide(reason);
  }
};

}  // namespace

class AutofillPopupControllerUnitTest : public ChromeRenderViewHostTestHarness {
 public:
  AutofillPopupControllerUnitTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AutofillPopupControllerUnitTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Make sure RenderFrame is created.
    NavigateAndCommit(GURL("about:blank"));
    FocusWebContentsOnMainFrame();
    ASSERT_TRUE(web_contents()->GetFocusedFrame());
    external_delegate_ = CreateExternalDelegate();
    autofill_popup_view_ = std::make_unique<NiceMock<MockAutofillPopupView>>();

#if BUILDFLAG(IS_ANDROID)
    autofill_popup_controller_ =
        (new NiceMock<TestAutofillPopupController>(
             external_delegate_->GetWeakPtrForTest(), web_contents(),
             gfx::RectF(), show_pwd_migration_warning_callback_.Get()))
            ->GetWeakPtr();
    ManualFillingControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_pwd_controller_.AsWeakPtr(),
        mock_address_controller_.AsWeakPtr(), mock_cc_controller_.AsWeakPtr(),
        std::make_unique<NiceMock<MockManualFillingView>>());
#else
    autofill_popup_controller_ =
        (new NiceMock<TestAutofillPopupController>(
             external_delegate_->GetWeakPtrForTest(), web_contents(),
             gfx::RectF(), base::DoNothing()))
            ->GetWeakPtr();
#endif
    autofill_popup_controller_->SetViewForTesting(
        autofill_popup_view()->GetWeakPtr());
  }

  void TearDown() override {
    // This will make sure the controller and the view (if any) are both
    // cleaned up.
    if (autofill_popup_controller_) {
      popup_controller().DoHide();
    }

    external_delegate_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  virtual std::unique_ptr<NiceMock<MockAutofillExternalDelegate>>
  CreateExternalDelegate() {
    return std::make_unique<NiceMock<MockAutofillExternalDelegate>>(
        &autofill_manager());
  }

  // Shows empty suggestions with the popup_item_id ids passed as
  // `popup_item_ids`.
  void ShowSuggestions(
      const std::vector<PopupItemId>& popup_item_ids,
      AutofillSuggestionTriggerSource trigger_source =
          AutofillSuggestionTriggerSource::kFormControlElementClicked) {
    std::vector<Suggestion> suggestions;
    suggestions.reserve(popup_item_ids.size());
    for (PopupItemId popup_item_id : popup_item_ids) {
      suggestions.emplace_back(u"", popup_item_id);
    }
    popup_controller().Show(std::move(suggestions), trigger_source);
  }

  void ShowSuggestions(
      std::vector<Suggestion> suggestions,
      AutofillSuggestionTriggerSource trigger_source =
          AutofillSuggestionTriggerSource::kFormControlElementClicked) {
    popup_controller().Show(std::move(suggestions), trigger_source);
  }

  TestAutofillPopupController& popup_controller() {
    return static_cast<TestAutofillPopupController&>(
        *autofill_popup_controller_);
  }

  NiceMock<MockAutofillExternalDelegate>* delegate() {
    return external_delegate_.get();
  }

  MockAutofillPopupView* autofill_popup_view() {
    return autofill_popup_view_.get();
  }

  content::NativeWebKeyboardEvent CreateTabKeyPressEvent() {
    content::NativeWebKeyboardEvent event(
        blink::WebInputEvent::Type::kRawKeyDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    event.dom_key = ui::DomKey::TAB;
    event.dom_code = static_cast<int>(ui::DomCode::TAB);
    event.native_key_code =
        ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::TAB);
    event.windows_key_code = ui::VKEY_TAB;
    return event;
  }

 protected:
  TestContentAutofillClient* autofill_client() {
    return autofill_client_injector_[web_contents()];
  }

  NiceMock<MockAutofillDriver>* autofill_driver() {
    return autofill_driver_injector_[web_contents()];
  }

  BrowserAutofillManager& autofill_manager() {
    return static_cast<BrowserAutofillManager&>(
        autofill_driver()->GetAutofillManager());
  }

  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<NiceMock<MockAutofillDriver>>
      autofill_driver_injector_;

  test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::unique_ptr<NiceMock<MockAutofillExternalDelegate>> external_delegate_;
  std::unique_ptr<NiceMock<MockAutofillPopupView>> autofill_popup_view_;
#if BUILDFLAG(IS_ANDROID)
  NiceMock<MockPasswordAccessoryController> mock_pwd_controller_;
  NiceMock<MockAddressAccessoryController> mock_address_controller_;
  NiceMock<MockCreditCardAccessoryController> mock_cc_controller_;
  base::MockCallback<base::RepeatingCallback<void(
      gfx::NativeWindow,
      Profile*,
      password_manager::metrics_util::PasswordMigrationWarningTriggers)>>
      show_pwd_migration_warning_callback_;
#endif
  base::WeakPtr<AutofillPopupControllerImpl> autofill_popup_controller_;
};

TEST_F(AutofillPopupControllerUnitTest, RemoveSuggestion) {
  ShowSuggestions({PopupItemId::kAddressEntry, PopupItemId::kAddressEntry,
                   PopupItemId::kAutofillOptions});

  // Generate a popup, so it can be hidden later. It doesn't matter what the
  // external_delegate thinks is being shown in the process, since we are just
  // testing the popup here.
  test::GenerateTestAutofillPopup(external_delegate_.get());

  // Remove the first entry. The popup should be redrawn since its size has
  // changed.
  EXPECT_CALL(popup_controller(), OnSuggestionsChanged());
  EXPECT_TRUE(popup_controller().RemoveSuggestion(0));
  Mock::VerifyAndClearExpectations(autofill_popup_view());

  // Remove the next entry. The popup should then be hidden since there are
  // no Autofill entries left.
  EXPECT_CALL(popup_controller(), Hide(PopupHidingReason::kNoSuggestions));
  EXPECT_TRUE(popup_controller().RemoveSuggestion(0));
}

TEST_F(AutofillPopupControllerUnitTest,
       RemoveAutocompleteSuggestion_AnnounceText) {
  base::HistogramTester histogram_tester;
  ShowSuggestions({Suggestion(u"main text", PopupItemId::kAutocompleteEntry)});
  test::GenerateTestAutofillPopup(external_delegate_.get());

  EXPECT_CALL(*autofill_popup_view(),
              AxAnnounce(Eq(u"Entry main text has been deleted")));
  EXPECT_TRUE(popup_controller().RemoveSuggestion(0));
}

TEST_F(AutofillPopupControllerUnitTest,
       RemoveAutocompleteSuggestion_IgnoresClickOutsideCheck) {
  ShowSuggestions(
      {PopupItemId::kAutocompleteEntry, PopupItemId::kAutocompleteEntry});

  // Generate a popup, so it can be hidden later. It doesn't matter what the
  // external_delegate thinks is being shown in the process, since we are just
  // testing the popup here.
  test::GenerateTestAutofillPopup(external_delegate_.get());

  // Remove the first entry. The popup should be redrawn since its size has
  // changed.
  EXPECT_CALL(popup_controller(), OnSuggestionsChanged());
  EXPECT_TRUE(popup_controller().RemoveSuggestion(0));
  Mock::VerifyAndClearExpectations(autofill_popup_view());

  EXPECT_TRUE(
      popup_controller().ShouldIgnoreMouseObservedOutsideItemBoundsCheck());
}

TEST_F(AutofillPopupControllerUnitTest,
       ManualFallBackTriggerSource_IgnoresClickOutsideCheck) {
  ShowSuggestions({PopupItemId::kAddressEntry},
                  AutofillSuggestionTriggerSource::
                      kManualFallbackForAutocompleteUnrecognized);

  // Generate a popup, so it can be hidden later. It doesn't matter what the
  // external_delegate thinks is being shown in the process, since we are just
  // testing the popup here.
  test::GenerateTestAutofillPopup(external_delegate_.get());

  EXPECT_TRUE(
      popup_controller().ShouldIgnoreMouseObservedOutsideItemBoundsCheck());
}

TEST_F(AutofillPopupControllerUnitTest, UpdateDataListValues) {
  ShowSuggestions({PopupItemId::kAddressEntry});

  // Add one data list entry.
  std::u16string value1 = u"data list value 1";
  std::vector<std::u16string> data_list_values{value1};
  std::u16string label1 = u"data list label 1";
  std::vector<std::u16string> data_list_labels{label1};

  popup_controller().UpdateDataListValues(data_list_values, data_list_labels);

  ASSERT_EQ(3, popup_controller().GetLineCount());

  Suggestion result0 = popup_controller().GetSuggestionAt(0);
  EXPECT_EQ(value1, result0.main_text.value);
  EXPECT_EQ(value1, popup_controller().GetSuggestionMainTextAt(0));
  ASSERT_EQ(1u, result0.labels.size());
  ASSERT_EQ(1u, result0.labels[0].size());
  EXPECT_EQ(label1, result0.labels[0][0].value);
  EXPECT_EQ(std::u16string(), result0.additional_label);
  EXPECT_EQ(label1, popup_controller().GetSuggestionLabelsAt(0)[0][0].value);
  EXPECT_EQ(PopupItemId::kDatalistEntry, result0.popup_item_id);

  Suggestion result1 = popup_controller().GetSuggestionAt(1);
  EXPECT_EQ(std::u16string(), result1.main_text.value);
  EXPECT_TRUE(result1.labels.empty());
  EXPECT_EQ(std::u16string(), result1.additional_label);
  EXPECT_EQ(PopupItemId::kSeparator, result1.popup_item_id);

  Suggestion result2 = popup_controller().GetSuggestionAt(2);
  EXPECT_EQ(std::u16string(), result2.main_text.value);
  EXPECT_TRUE(result2.labels.empty());
  EXPECT_EQ(std::u16string(), result2.additional_label);
  EXPECT_EQ(PopupItemId::kAddressEntry, result2.popup_item_id);

  // Add two data list entries (which should replace the current one).
  std::u16string value2 = u"data list value 2";
  data_list_values.push_back(value2);
  std::u16string label2 = u"data list label 2";
  data_list_labels.push_back(label2);

  popup_controller().UpdateDataListValues(data_list_values, data_list_labels);
  ASSERT_EQ(4, popup_controller().GetLineCount());

  // Original one first, followed by new one, then separator.
  EXPECT_EQ(value1, popup_controller().GetSuggestionAt(0).main_text.value);
  EXPECT_EQ(value1, popup_controller().GetSuggestionMainTextAt(0));
  ASSERT_EQ(1u, popup_controller().GetSuggestionAt(0).labels.size());
  ASSERT_EQ(1u, popup_controller().GetSuggestionAt(0).labels[0].size());
  EXPECT_EQ(label1, popup_controller().GetSuggestionAt(0).labels[0][0].value);
  EXPECT_EQ(std::u16string(),
            popup_controller().GetSuggestionAt(0).additional_label);
  EXPECT_EQ(value2, popup_controller().GetSuggestionAt(1).main_text.value);
  EXPECT_EQ(value2, popup_controller().GetSuggestionMainTextAt(1));
  ASSERT_EQ(1u, popup_controller().GetSuggestionAt(1).labels.size());
  ASSERT_EQ(1u, popup_controller().GetSuggestionAt(1).labels[0].size());
  EXPECT_EQ(label2, popup_controller().GetSuggestionAt(1).labels[0][0].value);
  EXPECT_EQ(std::u16string(),
            popup_controller().GetSuggestionAt(1).additional_label);
  EXPECT_EQ(PopupItemId::kSeparator,
            popup_controller().GetSuggestionAt(2).popup_item_id);

  // Clear all data list values.
  data_list_values.clear();
  popup_controller().UpdateDataListValues(data_list_values, data_list_labels);

  ASSERT_EQ(1, popup_controller().GetLineCount());
  EXPECT_EQ(PopupItemId::kAddressEntry,
            popup_controller().GetSuggestionAt(0).popup_item_id);
}

TEST_F(AutofillPopupControllerUnitTest, PopupsWithOnlyDataLists) {
  // Create the popup with a single datalist element.
  ShowSuggestions({PopupItemId::kDatalistEntry});

  // Replace the datalist element with a new one.
  std::u16string value1 = u"data list value 1";
  std::vector<std::u16string> data_list_values{value1};
  std::u16string label1 = u"data list label 1";
  std::vector<std::u16string> data_list_labels{label1};

  popup_controller().UpdateDataListValues(data_list_values, data_list_labels);

  ASSERT_EQ(1, popup_controller().GetLineCount());
  EXPECT_EQ(value1, popup_controller().GetSuggestionAt(0).main_text.value);
  ASSERT_EQ(1u, popup_controller().GetSuggestionAt(0).labels.size());
  ASSERT_EQ(1u, popup_controller().GetSuggestionAt(0).labels[0].size());
  EXPECT_EQ(label1, popup_controller().GetSuggestionAt(0).labels[0][0].value);
  EXPECT_EQ(std::u16string(),
            popup_controller().GetSuggestionAt(0).additional_label);
  EXPECT_EQ(PopupItemId::kDatalistEntry,
            popup_controller().GetSuggestionAt(0).popup_item_id);

  // Clear datalist values and check that the popup becomes hidden.
  EXPECT_CALL(popup_controller(), Hide(PopupHidingReason::kNoSuggestions));
  data_list_values.clear();
  popup_controller().UpdateDataListValues(data_list_values, data_list_values);
}

TEST_F(AutofillPopupControllerUnitTest, GetOrCreateAndroid) {
  NiceMock<MockAutofillExternalDelegate> delegate(&autofill_manager());

  WeakPtr<AutofillPopupControllerImpl> controller =
      AutofillPopupControllerImpl::GetOrCreate(
          WeakPtr<AutofillPopupControllerImpl>(), delegate.GetWeakPtrForTest(),
          web_contents(), nullptr, gfx::RectF(), base::i18n::UNKNOWN_DIRECTION);
  EXPECT_TRUE(controller.get());

  controller->Hide(PopupHidingReason::kViewDestroyed);

  controller = AutofillPopupControllerImpl::GetOrCreate(
      WeakPtr<AutofillPopupControllerImpl>(), delegate.GetWeakPtrForTest(),
      web_contents(), nullptr, gfx::RectF(), base::i18n::UNKNOWN_DIRECTION);
  EXPECT_TRUE(controller.get());

  WeakPtr<AutofillPopupControllerImpl> controller2 =
      AutofillPopupControllerImpl::GetOrCreate(
          controller, delegate.GetWeakPtrForTest(), web_contents(), nullptr,
          gfx::RectF(), base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(controller.get(), controller2.get());
  controller->Hide(PopupHidingReason::kViewDestroyed);
  NiceMock<TestAutofillPopupController>* test_controller =
      new NiceMock<TestAutofillPopupController>(delegate.GetWeakPtrForTest(),
                                                web_contents(), gfx::RectF(),
                                                base::DoNothing());
  EXPECT_CALL(*test_controller, Hide(PopupHidingReason::kViewDestroyed));

  gfx::RectF bounds(0.f, 0.f, 1.f, 2.f);
  base::WeakPtr<AutofillPopupControllerImpl> controller3 =
      AutofillPopupControllerImpl::GetOrCreate(
          test_controller->GetWeakPtr(), delegate.GetWeakPtrForTest(),
          web_contents(), nullptr, bounds, base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(bounds, static_cast<AutofillPopupController*>(controller3.get())
                        ->element_bounds());
  controller3->Hide(PopupHidingReason::kViewDestroyed);

  // Hide the test_controller to delete it.
  test_controller->DoHide();

  test_controller = new NiceMock<TestAutofillPopupController>(
      delegate.GetWeakPtrForTest(), web_contents(), gfx::RectF(),
      base::DoNothing());
  EXPECT_CALL(*test_controller, Hide).Times(0);

  const base::WeakPtr<AutofillPopupControllerImpl> controller4 =
      AutofillPopupControllerImpl::GetOrCreate(
          test_controller->GetWeakPtr(), delegate.GetWeakPtrForTest(),
          web_contents(), nullptr, bounds, base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(bounds,
            static_cast<const AutofillPopupController*>(controller4.get())
                ->element_bounds());
  delete test_controller;
}

TEST_F(AutofillPopupControllerUnitTest, ProperlyResetController) {
  ShowSuggestions(
      {PopupItemId::kAutocompleteEntry, PopupItemId::kAutocompleteEntry});

  // Now show a new popup with the same controller, but with fewer items.
  WeakPtr<AutofillPopupControllerImpl> controller =
      AutofillPopupControllerImpl::GetOrCreate(
          popup_controller().GetWeakPtr(), delegate()->GetWeakPtrForTest(),
          nullptr, nullptr, gfx::RectF(), base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(0, controller->GetLineCount());
}

TEST_F(AutofillPopupControllerUnitTest, HidingClearsPreview) {
  // Create a new controller, because hiding destroys it and we can't destroy it
  // twice.
  StrictMock<MockAutofillExternalDelegate> delegate(&autofill_manager());
  StrictMock<TestAutofillPopupController>* test_controller =
      new StrictMock<TestAutofillPopupController>(delegate.GetWeakPtrForTest(),
                                                  web_contents(), gfx::RectF(),
                                                  base::DoNothing());
  EXPECT_CALL(delegate, ClearPreviewedForm());
  // Hide() also deletes the object itself.
  test_controller->DoHide();
}

TEST_F(AutofillPopupControllerUnitTest, DontHideWhenWaitingForData) {
  EXPECT_CALL(*autofill_popup_view(), Hide).Times(0);
  popup_controller().PinView();

  // Hide() will not work for stale data or when focusing native UI.
  popup_controller().DoHide(PopupHidingReason::kStaleData);
  popup_controller().DoHide(PopupHidingReason::kEndEditing);

  // Check the expectations now since TearDown will perform a successful hide.
  Mock::VerifyAndClearExpectations(delegate());
  Mock::VerifyAndClearExpectations(autofill_popup_view());
}

TEST_F(AutofillPopupControllerUnitTest, ShouldReportHidingPopupReason) {
  // Create a new controller, because hiding destroys it and we can't destroy it
  // twice (since we already hide it in the destructor).
  NiceMock<MockAutofillExternalDelegate> delegate(&autofill_manager());
  NiceMock<TestAutofillPopupController>* test_controller =
      new NiceMock<TestAutofillPopupController>(delegate.GetWeakPtrForTest(),
                                                web_contents(), gfx::RectF(),
                                                base::DoNothing());
  base::HistogramTester histogram_tester;
  // DoHide() invokes Hide() that also deletes the object itself.
  test_controller->DoHide(PopupHidingReason::kTabGone);

  histogram_tester.ExpectTotalCount("Autofill.PopupHidingReason", 1);
  histogram_tester.ExpectBucketCount("Autofill.PopupHidingReason",
                                     /*kTabGone=*/8, 1);
}

// This is a regression test for crbug.com/521133 to ensure that we don't crash
// when suggestions updates race with user selections.
TEST_F(AutofillPopupControllerUnitTest, SelectInvalidSuggestion) {
  ShowSuggestions({PopupItemId::kAddressEntry});

  EXPECT_CALL(*delegate(), DidAcceptSuggestion).Times(0);

  // The following should not crash:
  popup_controller()
      .AcceptSuggestion(                             /*index=*/
                        1, base::TimeTicks::Now());  // Out of bounds!
}

TEST_F(AutofillPopupControllerUnitTest, AcceptSuggestionRespectsTimeout) {
  base::HistogramTester histogram_tester;
  ShowSuggestions({PopupItemId::kAddressEntry});

  // Calls before the threshold are ignored.
  EXPECT_CALL(*delegate(), DidAcceptSuggestion).Times(0);
  popup_controller().AcceptSuggestion(0, base::TimeTicks::Now());
  task_environment()->FastForwardBy(base::Milliseconds(100));
  popup_controller().AcceptSuggestion(/*index=*/0, base::TimeTicks::Now());

  EXPECT_CALL(*delegate(), DidAcceptSuggestion);
  task_environment()->FastForwardBy(base::Milliseconds(400));
  popup_controller().AcceptSuggestion(/*index=*/0, base::TimeTicks::Now());

  histogram_tester.ExpectTotalCount(
      "Autofill.Popup.AcceptanceDelayThresholdNotMet", 2);
}

TEST_F(AutofillPopupControllerUnitTest, AcceptSuggestionWithoutThreshold) {
  base::HistogramTester histogram_tester;
  ShowSuggestions({PopupItemId::kAddressEntry});

  // Calls are accepted immediately.
  EXPECT_CALL(*delegate(), DidAcceptSuggestion).Times(1);
  popup_controller().AcceptSuggestionWithoutThreshold(0);
  histogram_tester.ExpectTotalCount(
      "Autofill.Popup.AcceptanceDelayThresholdNotMet", 0);
}

TEST_F(AutofillPopupControllerUnitTest,
       AcceptSuggestionTimeoutIsUpdatedOnPopupMove) {
  base::HistogramTester histogram_tester;
  ShowSuggestions({PopupItemId::kAddressEntry});

  // Calls before the threshold are ignored.
  EXPECT_CALL(*delegate(), DidAcceptSuggestion).Times(0);
  popup_controller().AcceptSuggestion(/*index=*/0, base::TimeTicks::Now());
  task_environment()->FastForwardBy(base::Milliseconds(100));
  popup_controller().AcceptSuggestion(/*index=*/0, base::TimeTicks::Now());

  histogram_tester.ExpectTotalCount(
      "Autofill.Popup.AcceptanceDelayThresholdNotMet", 2);
  task_environment()->FastForwardBy(base::Milliseconds(400));
  // Show the suggestions again (simulating, e.g., a click somewhere slightly
  // different).
  ShowSuggestions({PopupItemId::kAddressEntry});

  EXPECT_CALL(*delegate(), DidAcceptSuggestion).Times(0);
  popup_controller().AcceptSuggestion(/*index=*/0, base::TimeTicks::Now());
  histogram_tester.ExpectTotalCount(
      "Autofill.Popup.AcceptanceDelayThresholdNotMet", 3);

  EXPECT_CALL(*delegate(), DidAcceptSuggestion);
  // After waiting, suggestions are accepted again.
  task_environment()->FastForwardBy(base::Milliseconds(500));
  popup_controller().AcceptSuggestion(/*index=*/0, base::TimeTicks::Now());
  histogram_tester.ExpectTotalCount(
      "Autofill.Popup.AcceptanceDelayThresholdNotMet", 3);
}

// Tests that when a picture-in-picture window is initialized, there is a call
// to the popup view to check if the autofill popup bounds overlap with the
// picture-in-picture window.
TEST_F(AutofillPopupControllerUnitTest,
       CheckBoundsOverlapWithPictureInPicture) {
  EXPECT_CALL(*autofill_popup_view(), OverlapsWithPictureInPictureWindow)
      .Times(1);
  PictureInPictureWindowManager* picture_in_picture_window_manager =
      PictureInPictureWindowManager::GetInstance();
  picture_in_picture_window_manager->EnterVideoPictureInPicture(web_contents());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(AutofillPopupControllerUnitTest,
       AcceptPwdSuggestionInvokesWarningAndroid) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  ShowSuggestions({PopupItemId::kPasswordEntry});

  // Calls are accepted immediately.
  EXPECT_CALL(*delegate(), DidAcceptSuggestion).Times(1);
  EXPECT_CALL(show_pwd_migration_warning_callback_,
              Run(_, _,
                  password_manager::metrics_util::
                      PasswordMigrationWarningTriggers::kKeyboardAcessoryBar));
  popup_controller().AcceptSuggestionWithoutThreshold(0);
}

TEST_F(AutofillPopupControllerUnitTest,
       AcceptUsernameSuggestionInvokesWarningAndroid) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  ShowSuggestions({PopupItemId::kUsernameEntry});

  // Calls are accepted immediately.
  EXPECT_CALL(*delegate(), DidAcceptSuggestion).Times(1);
  EXPECT_CALL(show_pwd_migration_warning_callback_, Run);
  popup_controller().AcceptSuggestionWithoutThreshold(0);
}

TEST_F(AutofillPopupControllerUnitTest,
       AcceptPwdSuggestionNoWarningIfDisabledAndroid) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  ShowSuggestions({PopupItemId::kPasswordEntry});

  // Calls are accepted immediately.
  EXPECT_CALL(*delegate(), DidAcceptSuggestion).Times(1);
  EXPECT_CALL(show_pwd_migration_warning_callback_, Run).Times(0);
  popup_controller().AcceptSuggestionWithoutThreshold(0);
}

TEST_F(AutofillPopupControllerUnitTest, AcceptAddressNoPwdWarningAndroid) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  ShowSuggestions({PopupItemId::kAddressEntry});

  // Calls are accepted immediately.
  EXPECT_CALL(*delegate(), DidAcceptSuggestion).Times(1);
  EXPECT_CALL(show_pwd_migration_warning_callback_, Run).Times(0);
  popup_controller().AcceptSuggestionWithoutThreshold(0);
}
#endif

#if !BUILDFLAG(IS_ANDROID)
TEST_F(AutofillPopupControllerUnitTest, SubPopupIsCreatedWithViewFromParent) {
  NiceMock<MockAutofillPopupView> autofill_popup_sub_view;
  EXPECT_CALL(*autofill_popup_view(), CreateSubPopupView)
      .WillRepeatedly(Return(autofill_popup_sub_view.GetWeakPtr()));
  base::WeakPtr<AutofillPopupController> sub_controller =
      popup_controller().OpenSubPopup({0, 0, 10, 10}, {});
  EXPECT_TRUE(sub_controller);
}
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
class MockAxTreeManager : public ui::AXTreeManager {
 public:
  MockAxTreeManager() = default;
  MockAxTreeManager(MockAxTreeManager&) = delete;
  MockAxTreeManager& operator=(MockAxTreeManager&) = delete;
  ~MockAxTreeManager() override = default;

  MOCK_METHOD(ui::AXNode*,
              GetNodeFromTree,
              (const ui::AXTreeID& tree_id, const int32_t node_id),
              (const override));
  MOCK_METHOD(ui::AXPlatformNodeDelegate*,
              GetDelegate,
              (const ui::AXTreeID tree_id, const int32_t node_id),
              (const override));
  MOCK_METHOD(ui::AXPlatformNodeDelegate*,
              GetRootDelegate,
              (const ui::AXTreeID tree_id),
              (const override));
  MOCK_METHOD(ui::AXTreeID, GetTreeID, (), (const override));
  MOCK_METHOD(ui::AXTreeID, GetParentTreeID, (), (const override));
  MOCK_METHOD(ui::AXNode*, GetRootAsAXNode, (), (const override));
  MOCK_METHOD(ui::AXNode*, GetParentNodeFromParentTree, (), (const override));
};

class MockAxPlatformNodeDelegate : public ui::AXPlatformNodeDelegate {
 public:
  MockAxPlatformNodeDelegate() = default;
  MockAxPlatformNodeDelegate(MockAxPlatformNodeDelegate&) = delete;
  MockAxPlatformNodeDelegate& operator=(MockAxPlatformNodeDelegate&) = delete;
  ~MockAxPlatformNodeDelegate() override = default;

  MOCK_METHOD(ui::AXPlatformNode*, GetFromNodeID, (int32_t id), (override));
  MOCK_METHOD(ui::AXPlatformNode*,
              GetFromTreeIDAndNodeID,
              (const ui::AXTreeID& tree_id, int32_t id),
              (override));
};

class MockAxPlatformNode : public ui::AXPlatformNodeBase {
 public:
  MockAxPlatformNode() = default;
  MockAxPlatformNode(MockAxPlatformNode&) = delete;
  MockAxPlatformNode& operator=(MockAxPlatformNode&) = delete;
  ~MockAxPlatformNode() override = default;

  MOCK_METHOD(ui::AXPlatformNodeDelegate*, GetDelegate, (), (const override));
};

class AutofillPopupControllerAccessibilityUnitTest
    : public AutofillPopupControllerUnitTest {
 public:
  static constexpr int kAxUniqueId = 123;

  AutofillPopupControllerAccessibilityUnitTest()
      : accessibility_mode_override_(ui::AXMode::kScreenReader) {}
  AutofillPopupControllerAccessibilityUnitTest(
      AutofillPopupControllerAccessibilityUnitTest&) = delete;
  AutofillPopupControllerAccessibilityUnitTest& operator=(
      AutofillPopupControllerAccessibilityUnitTest&) = delete;
  ~AutofillPopupControllerAccessibilityUnitTest() override = default;

  void SetUp() override {
    AutofillPopupControllerUnitTest::SetUp();

    ON_CALL(*autofill_driver(), GetAxTreeId())
        .WillByDefault(Return(test_tree_id_));
    ON_CALL(popup_controller(), GetRootAXPlatformNodeForWebContents)
        .WillByDefault(Return(&mock_ax_platform_node_));
    ON_CALL(mock_ax_platform_node_, GetDelegate)
        .WillByDefault(Return(&mock_ax_platform_node_delegate_));
    ON_CALL(*autofill_popup_view_, GetAxUniqueId)
        .WillByDefault(Return(absl::optional<int32_t>(kAxUniqueId)));
    ON_CALL(mock_ax_platform_node_delegate_, GetFromTreeIDAndNodeID)
        .WillByDefault(Return(&mock_ax_platform_node_));
  }

  void TearDown() override {
    // This needs to bo reset explicit because having the mode set to
    // `kScreenReader` causes mocked functions to get called  with
    // `mock_ax_platform_node_delegate` after it has been destroyed.
    accessibility_mode_override_.ResetMode();
    AutofillPopupControllerUnitTest::TearDown();
  }

 protected:
  content::ScopedAccessibilityModeOverride accessibility_mode_override_;
  MockAxPlatformNodeDelegate mock_ax_platform_node_delegate_;
  MockAxPlatformNode mock_ax_platform_node_;
  ui::AXTreeID test_tree_id_ = ui::AXTreeID::CreateNewAXTreeID();
};

// Test for successfully firing controls changed event for popup show/hide.
TEST_F(AutofillPopupControllerAccessibilityUnitTest,
       FireControlsChangedEventDuringShowAndHide) {
  ShowSuggestions({PopupItemId::kAddressEntry});
  // Manually fire the event for popup show since setting the test view results
  // in the fire controls changed event not being sent.
  popup_controller().FireControlsChangedEvent(true);
  EXPECT_EQ(kAxUniqueId, ui::GetActivePopupAxUniqueId());

  popup_controller().DoHide();
  EXPECT_EQ(absl::nullopt, ui::GetActivePopupAxUniqueId());
}

// Test for attempting to fire controls changed event when ax tree manager
// fails to retrieve the ax platform node associated with the popup.
// No event is fired and global active popup ax unique id is not set.
TEST_F(AutofillPopupControllerAccessibilityUnitTest,
       FireControlsChangedEventNoAxPlatformNode) {
  EXPECT_CALL(mock_ax_platform_node_delegate_, GetFromTreeIDAndNodeID)
      .WillOnce(Return(nullptr));

  ShowSuggestions({PopupItemId::kAddressEntry});
  // Manually fire the event for popup show since setting the test view results
  // in the fire controls changed event not being sent.
  popup_controller().FireControlsChangedEvent(true);
  EXPECT_EQ(absl::nullopt, ui::GetActivePopupAxUniqueId());
}

// Test for attempting to fire controls changed event when failing to retrieve
// the autofill popup's ax unique id. No event is fired and the global active
// popup ax unique id is not set.
TEST_F(AutofillPopupControllerAccessibilityUnitTest,
       FireControlsChangedEventNoPopupAxUniqueId) {
  EXPECT_CALL(*autofill_popup_view_, GetAxUniqueId)
      .WillOnce(testing::Return(absl::nullopt));

  ShowSuggestions({PopupItemId::kAddressEntry});
  // Manually fire the event for popup show since setting the test view results
  // in the fire controls changed event not being sent.
  popup_controller().FireControlsChangedEvent(true);
  EXPECT_EQ(absl::nullopt, ui::GetActivePopupAxUniqueId());
}
#endif

}  // namespace autofill
