// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
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
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "content/public/test/navigation_simulator.h"
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
#include "chrome/browser/autofill/manual_filling_controller_impl.h"
#include "chrome/browser/autofill/mock_address_accessory_controller.h"
#include "chrome/browser/autofill/mock_credit_card_accessory_controller.h"
#include "chrome/browser/autofill/mock_manual_filling_view.h"
#include "chrome/browser/autofill/mock_password_accessory_controller.h"
#endif

namespace autofill {
namespace {

using base::ASCIIToUTF16;
using base::WeakPtr;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Return;
using ::testing::StrictMock;

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
  MOCK_METHOD(void, OnPopupShown, (), (override));
  MOCK_METHOD(void, OnPopupHidden, (), (override));
  MOCK_METHOD(void,
              DidAcceptSuggestion,
              (const Suggestion&, int, AutofillSuggestionTriggerSource),
              (override));
  MOCK_METHOD(void,
              DidPerformButtonActionForSuggestion,
              (const Suggestion&),
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
  MOCK_METHOD(std::optional<AutofillClient::PopupScreenLocation>,
              GetPopupScreenLocation,
              (),
              (const override));

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
  using AutofillPopupControllerImpl::element_bounds;
  using AutofillPopupControllerImpl::FireControlsChangedEvent;
  using AutofillPopupControllerImpl::GetLineCount;
  using AutofillPopupControllerImpl::GetRootAXPlatformNodeForWebContents;
  using AutofillPopupControllerImpl::GetSuggestionAt;
  using AutofillPopupControllerImpl::GetSuggestionLabelsAt;
  using AutofillPopupControllerImpl::GetSuggestionMainTextAt;
  using AutofillPopupControllerImpl::GetWeakPtr;
  using AutofillPopupControllerImpl::PerformButtonActionForSuggestion;
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

class BrowserAutofillManagerWithMockDelegate : public BrowserAutofillManager {
 public:
  BrowserAutofillManagerWithMockDelegate(AutofillDriver* driver,
                                         ContentAutofillClient* client)
      : BrowserAutofillManager(driver, client, "en-US") {
    test_api(*this).SetExternalDelegate(
        std::make_unique<NiceMock<MockAutofillExternalDelegate>>(this));
  }

  BrowserAutofillManagerWithMockDelegate(
      BrowserAutofillManagerWithMockDelegate&) = delete;
  BrowserAutofillManagerWithMockDelegate& operator=(
      BrowserAutofillManagerWithMockDelegate&) = delete;

  ~BrowserAutofillManagerWithMockDelegate() override = default;

  NiceMock<MockAutofillExternalDelegate>& external_delegate() {
    return static_cast<NiceMock<MockAutofillExternalDelegate>&>(
        *test_api(*this).external_delegate());
  }
};

class TestContentAutofillClientWithMockController
    : public TestContentAutofillClient {
 public:
  explicit TestContentAutofillClientWithMockController(
      content::WebContents* web_contents)
      : TestContentAutofillClient(web_contents) {
    ON_CALL(popup_view(), CreateSubPopupView)
        .WillByDefault(Return(sub_popup_view().GetWeakPtr()));
  }

  ~TestContentAutofillClientWithMockController() override { DoHide(); }

  // Returns the current controller. Controllers are specific to the `manager`'s
  // AutofillExternalDelegate. Therefore, when there are two consecutive
  // `popup_controller(x)` and `popup_controller(y)`, the second call hides the
  // old and creates new controller iff `x` and `y` are distinct.
  NiceMock<TestAutofillPopupController>& popup_controller(
      BrowserAutofillManagerWithMockDelegate& manager) {
    if (manager_of_last_controller_.get() != &manager) {
      DoHide();
      CHECK(!popup_controller_);
    }
    if (!popup_controller_) {
      popup_controller_ = (new NiceMock<TestAutofillPopupController>(
                               manager.external_delegate().GetWeakPtrForTest(),
                               &GetWebContents(), gfx::RectF(),
                               show_pwd_migration_warning_callback_.Get()))
                              ->GetWeakPtr();
      popup_controller_->SetViewForTesting(popup_view_->GetWeakPtr());
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
#endif

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

  NiceMock<TestAutofillPopupController>& cast_popup_controller() {
    return static_cast<NiceMock<TestAutofillPopupController>&>(
        *popup_controller_);
  }

  base::WeakPtr<AutofillPopupControllerImpl> popup_controller_;
  base::WeakPtr<AutofillManager> manager_of_last_controller_;

  std::unique_ptr<NiceMock<MockAutofillPopupView>> popup_view_ =
      std::make_unique<NiceMock<MockAutofillPopupView>>();
  std::unique_ptr<NiceMock<MockAutofillPopupView>> sub_popup_view_ =
      std::make_unique<NiceMock<MockAutofillPopupView>>();
  base::MockCallback<base::RepeatingCallback<void(
      gfx::NativeWindow,
      Profile*,
      password_manager::metrics_util::PasswordMigrationWarningTriggers)>>
      show_pwd_migration_warning_callback_;
};

content::RenderFrameHost* CreateAndNavigateChildFrame(
    content::RenderFrameHost* parent,
    const GURL& url,
    std::string_view name) {
  content::RenderFrameHost* rfh =
      content::RenderFrameHostTester::For(parent)->AppendChild(
          std::string(name));
  // ContentAutofillDriverFactory::DidFinishNavigation() creates a driver for
  // subframes only if
  // `NavigationHandle::HasSubframeNavigationEntryCommitted()` is true. This
  // is not the case for the first navigation. (In non-unit-tests, the first
  // navigation creates a driver in
  // ContentAutofillDriverFactory::BindAutofillDriver().) Therefore,
  // we simulate *two* navigations here, and explicitly set the transition
  // type for the second navigation.
  std::unique_ptr<content::NavigationSimulator> simulator;
  // First navigation: `HasSubframeNavigationEntryCommitted() == false`.
  // Must be a different URL from the second navigation.
  GURL about_blank("about:blank");
  CHECK_NE(about_blank, url);
  simulator =
      content::NavigationSimulator::CreateRendererInitiated(about_blank, rfh);
  simulator->Commit();
  rfh = simulator->GetFinalRenderFrameHost();
  // Second navigation: `HasSubframeNavigationEntryCommitted() == true`.
  // Must set the transition type to ui::PAGE_TRANSITION_MANUAL_SUBFRAME.
  simulator = content::NavigationSimulator::CreateRendererInitiated(url, rfh);
  simulator->SetTransition(ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
  simulator->Commit();
  return simulator->GetFinalRenderFrameHost();
}

content::RenderFrameHost* NavigateAndCommitFrame(content::RenderFrameHost* rfh,
                                                 const GURL& url) {
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(url, rfh);
  simulator->Commit();
  return simulator->GetFinalRenderFrameHost();
}

}  // namespace

class AutofillPopupControllerImplTest : public ChromeRenderViewHostTestHarness {
 public:
  AutofillPopupControllerImplTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AutofillPopupControllerImplTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://foo.com/"));
    FocusWebContentsOnMainFrame();
    ASSERT_TRUE(web_contents()->GetFocusedFrame());

#if BUILDFLAG(IS_ANDROID)
    ManualFillingControllerImpl::CreateForWebContentsForTesting(
        web_contents(), mock_pwd_controller_.AsWeakPtr(),
        mock_address_controller_.AsWeakPtr(), mock_cc_controller_.AsWeakPtr(),
        std::make_unique<NiceMock<MockManualFillingView>>());
#endif
  }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  TestContentAutofillClientWithMockController& client() {
    return *autofill_client_injector_[web_contents()];
  }

  NiceMock<MockAutofillDriver>& driver(
      content::RenderFrameHost* rfh = nullptr) {
    return *autofill_driver_injector_
        [rfh ? rfh : web_contents()->GetPrimaryMainFrame()];
  }

  BrowserAutofillManagerWithMockDelegate& manager(
      content::RenderFrameHost* rfh = nullptr) {
    return *autofill_manager_injector_[rfh ? rfh : main_frame()];
  }

  // Shows empty suggestions with the popup_item_id ids passed as
  // `popup_item_ids`.
  void ShowSuggestions(
      BrowserAutofillManagerWithMockDelegate& manager,
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
      BrowserAutofillManagerWithMockDelegate& manager,
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
  test::AutofillUnitTestEnvironment autofill_test_environment_;

  TestAutofillClientInjector<TestContentAutofillClientWithMockController>
      autofill_client_injector_;
  TestAutofillDriverInjector<NiceMock<MockAutofillDriver>>
      autofill_driver_injector_;
  TestAutofillManagerInjector<BrowserAutofillManagerWithMockDelegate>
      autofill_manager_injector_;

#if BUILDFLAG(IS_ANDROID)
  NiceMock<MockPasswordAccessoryController> mock_pwd_controller_;
  NiceMock<MockAddressAccessoryController> mock_address_controller_;
  NiceMock<MockCreditCardAccessoryController> mock_cc_controller_;
#endif
};

TEST_F(AutofillPopupControllerImplTest, RemoveSuggestion) {
  ShowSuggestions(manager(),
                  {PopupItemId::kAddressEntry, PopupItemId::kAddressEntry,
                   PopupItemId::kAutofillOptions});

  // Generate a popup, so it can be hidden later. It doesn't matter what the
  // external_delegate thinks is being shown in the process, since we are just
  // testing the popup here.
  test::GenerateTestAutofillPopup(&manager().external_delegate());

  // Remove the first entry. The popup should be redrawn since its size has
  // changed.
  EXPECT_CALL(client().popup_controller(manager()), OnSuggestionsChanged());
  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(0));
  Mock::VerifyAndClearExpectations(&client().popup_view());

  // Remove the next entry. The popup should then be hidden since there are
  // no Autofill entries left.
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kNoSuggestions));
  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(0));
}

TEST_F(AutofillPopupControllerImplTest,
       RemoveAutocompleteSuggestion_AnnounceText) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(),
                  {Suggestion(u"main text", PopupItemId::kAutocompleteEntry)});
  test::GenerateTestAutofillPopup(&manager().external_delegate());

  EXPECT_CALL(client().popup_view(),
              AxAnnounce(Eq(u"Entry main text has been deleted")));
  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(0));
}

TEST_F(AutofillPopupControllerImplTest,
       RemoveAutocompleteSuggestion_IgnoresClickOutsideCheck) {
  ShowSuggestions(manager(), {PopupItemId::kAutocompleteEntry,
                              PopupItemId::kAutocompleteEntry});

  // Generate a popup, so it can be hidden later. It doesn't matter what the
  // external_delegate thinks is being shown in the process, since we are just
  // testing the popup here.
  test::GenerateTestAutofillPopup(&manager().external_delegate());

  // Remove the first entry. The popup should be redrawn since its size has
  // changed.
  EXPECT_CALL(client().popup_controller(manager()), OnSuggestionsChanged());
  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(0));
  Mock::VerifyAndClearExpectations(&client().popup_view());

  EXPECT_TRUE(client()
                  .popup_controller(manager())
                  .ShouldIgnoreMouseObservedOutsideItemBoundsCheck());
}

TEST_F(AutofillPopupControllerImplTest,
       ManualFallBackTriggerSource_IgnoresClickOutsideCheck) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry},
                  AutofillSuggestionTriggerSource::kManualFallbackAddress);

  // Generate a popup, so it can be hidden later. It doesn't matter what the
  // external_delegate thinks is being shown in the process, since we are just
  // testing the popup here.
  test::GenerateTestAutofillPopup(&manager().external_delegate());

  EXPECT_TRUE(client()
                  .popup_controller(manager())
                  .ShouldIgnoreMouseObservedOutsideItemBoundsCheck());
}

TEST_F(AutofillPopupControllerImplTest, UpdateDataListValues) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  std::vector<SelectOption> options = {
      {.value = u"data list value 1", .content = u"data list label 1"}};
  client().popup_controller(manager()).UpdateDataListValues(options);

  ASSERT_EQ(3, client().popup_controller(manager()).GetLineCount());

  Suggestion result0 = client().popup_controller(manager()).GetSuggestionAt(0);
  EXPECT_EQ(options[0].value, result0.main_text.value);
  EXPECT_EQ(options[0].value,
            client().popup_controller(manager()).GetSuggestionMainTextAt(0));
  ASSERT_EQ(1u, result0.labels.size());
  ASSERT_EQ(1u, result0.labels[0].size());
  EXPECT_EQ(options[0].content, result0.labels[0][0].value);
  EXPECT_EQ(std::u16string(), result0.additional_label);
  EXPECT_EQ(options[0].content, client()
                                    .popup_controller(manager())
                                    .GetSuggestionLabelsAt(0)[0][0]
                                    .value);
  EXPECT_EQ(PopupItemId::kDatalistEntry, result0.popup_item_id);

  Suggestion result1 = client().popup_controller(manager()).GetSuggestionAt(1);
  EXPECT_EQ(std::u16string(), result1.main_text.value);
  EXPECT_TRUE(result1.labels.empty());
  EXPECT_EQ(std::u16string(), result1.additional_label);
  EXPECT_EQ(PopupItemId::kSeparator, result1.popup_item_id);

  Suggestion result2 = client().popup_controller(manager()).GetSuggestionAt(2);
  EXPECT_EQ(std::u16string(), result2.main_text.value);
  EXPECT_TRUE(result2.labels.empty());
  EXPECT_EQ(std::u16string(), result2.additional_label);
  EXPECT_EQ(PopupItemId::kAddressEntry, result2.popup_item_id);

  // Add two data list entries (which should replace the current one).
  options.push_back(
      {.value = u"data list value 1", .content = u"data list label 1"});
  client().popup_controller(manager()).UpdateDataListValues(options);
  ASSERT_EQ(4, client().popup_controller(manager()).GetLineCount());

  // Original one first, followed by new one, then separator.
  EXPECT_EQ(
      options[0].value,
      client().popup_controller(manager()).GetSuggestionAt(0).main_text.value);
  EXPECT_EQ(options[0].value,
            client().popup_controller(manager()).GetSuggestionMainTextAt(0));
  ASSERT_EQ(
      1u,
      client().popup_controller(manager()).GetSuggestionAt(0).labels.size());
  ASSERT_EQ(
      1u,
      client().popup_controller(manager()).GetSuggestionAt(0).labels[0].size());
  EXPECT_EQ(options[0].content, client()
                                    .popup_controller(manager())
                                    .GetSuggestionAt(0)
                                    .labels[0][0]
                                    .value);
  EXPECT_EQ(
      std::u16string(),
      client().popup_controller(manager()).GetSuggestionAt(0).additional_label);
  EXPECT_EQ(
      options[1].value,
      client().popup_controller(manager()).GetSuggestionAt(1).main_text.value);
  EXPECT_EQ(options[1].value,
            client().popup_controller(manager()).GetSuggestionMainTextAt(1));
  ASSERT_EQ(
      1u,
      client().popup_controller(manager()).GetSuggestionAt(1).labels.size());
  ASSERT_EQ(
      1u,
      client().popup_controller(manager()).GetSuggestionAt(1).labels[0].size());
  EXPECT_EQ(options[1].content, client()
                                    .popup_controller(manager())
                                    .GetSuggestionAt(1)
                                    .labels[0][0]
                                    .value);
  EXPECT_EQ(
      std::u16string(),
      client().popup_controller(manager()).GetSuggestionAt(1).additional_label);
  EXPECT_EQ(
      PopupItemId::kSeparator,
      client().popup_controller(manager()).GetSuggestionAt(2).popup_item_id);

  // Clear all data list values.
  options.clear();
  client().popup_controller(manager()).UpdateDataListValues(options);

  ASSERT_EQ(1, client().popup_controller(manager()).GetLineCount());
  EXPECT_EQ(
      PopupItemId::kAddressEntry,
      client().popup_controller(manager()).GetSuggestionAt(0).popup_item_id);
}

TEST_F(AutofillPopupControllerImplTest, PopupsWithOnlyDataLists) {
  // Create the popup with a single datalist element.
  ShowSuggestions(manager(), {PopupItemId::kDatalistEntry});

  // Replace the datalist element with a new one.
  std::vector<SelectOption> options = {
      {.value = u"data list value 1", .content = u"data list label 1"}};
  client().popup_controller(manager()).UpdateDataListValues(options);

  ASSERT_EQ(1, client().popup_controller(manager()).GetLineCount());
  EXPECT_EQ(
      options[0].value,
      client().popup_controller(manager()).GetSuggestionAt(0).main_text.value);
  ASSERT_EQ(
      1u,
      client().popup_controller(manager()).GetSuggestionAt(0).labels.size());
  ASSERT_EQ(
      1u,
      client().popup_controller(manager()).GetSuggestionAt(0).labels[0].size());
  EXPECT_EQ(options[0].content, client()
                                    .popup_controller(manager())
                                    .GetSuggestionAt(0)
                                    .labels[0][0]
                                    .value);
  EXPECT_EQ(
      std::u16string(),
      client().popup_controller(manager()).GetSuggestionAt(0).additional_label);
  EXPECT_EQ(
      PopupItemId::kDatalistEntry,
      client().popup_controller(manager()).GetSuggestionAt(0).popup_item_id);

  // Clear datalist values and check that the popup becomes hidden.
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kNoSuggestions));
  options.clear();
  client().popup_controller(manager()).UpdateDataListValues(options);
}

TEST_F(AutofillPopupControllerImplTest, GetOrCreateAndroid) {
  WeakPtr<AutofillPopupControllerImpl> controller =
      AutofillPopupControllerImpl::GetOrCreate(
          WeakPtr<AutofillPopupControllerImpl>(),
          manager().external_delegate().GetWeakPtrForTest(), web_contents(),
          nullptr, gfx::RectF(), base::i18n::UNKNOWN_DIRECTION);
  EXPECT_TRUE(controller);

  controller->Hide(PopupHidingReason::kViewDestroyed);
  EXPECT_FALSE(controller);

  controller = AutofillPopupControllerImpl::GetOrCreate(
      WeakPtr<AutofillPopupControllerImpl>(),
      manager().external_delegate().GetWeakPtrForTest(), web_contents(),
      nullptr, gfx::RectF(), base::i18n::UNKNOWN_DIRECTION);
  EXPECT_TRUE(controller);

  WeakPtr<AutofillPopupControllerImpl> controller2 =
      AutofillPopupControllerImpl::GetOrCreate(
          controller, manager().external_delegate().GetWeakPtrForTest(),
          web_contents(), nullptr, gfx::RectF(), base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(controller.get(), controller2.get());

  controller->Hide(PopupHidingReason::kViewDestroyed);
  EXPECT_FALSE(controller);
  EXPECT_FALSE(controller2);

  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kViewDestroyed));
  gfx::RectF bounds(0.f, 0.f, 1.f, 2.f);
  base::WeakPtr<AutofillPopupControllerImpl> controller3 =
      AutofillPopupControllerImpl::GetOrCreate(
          client().popup_controller(manager()).GetWeakPtr(),
          manager().external_delegate().GetWeakPtrForTest(), web_contents(),
          nullptr, bounds, base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(&client().popup_controller(manager()), controller3.get());
  EXPECT_EQ(bounds, static_cast<AutofillPopupController*>(controller3.get())
                        ->element_bounds());
  controller3->Hide(PopupHidingReason::kViewDestroyed);

  client().popup_controller(manager()).DoHide();

  const base::WeakPtr<AutofillPopupControllerImpl> controller4 =
      AutofillPopupControllerImpl::GetOrCreate(
          client().popup_controller(manager()).GetWeakPtr(),
          manager().external_delegate().GetWeakPtrForTest(), web_contents(),
          nullptr, bounds, base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(&client().popup_controller(manager()), controller4.get());
  EXPECT_EQ(bounds,
            static_cast<const AutofillPopupController*>(controller4.get())
                ->element_bounds());

  client().popup_controller(manager()).DoHide();
}

TEST_F(AutofillPopupControllerImplTest, ProperlyResetController) {
  ShowSuggestions(manager(), {PopupItemId::kAutocompleteEntry,
                              PopupItemId::kAutocompleteEntry});

  // Now show a new popup with the same controller, but with fewer items.
  WeakPtr<AutofillPopupControllerImpl> controller =
      AutofillPopupControllerImpl::GetOrCreate(
          client().popup_controller(manager()).GetWeakPtr(),
          manager().external_delegate().GetWeakPtrForTest(), nullptr, nullptr,
          gfx::RectF(), base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(0, controller->GetLineCountForTesting());
}

TEST_F(AutofillPopupControllerImplTest, HidingClearsPreview) {
  EXPECT_CALL(manager().external_delegate(), ClearPreviewedForm());
  EXPECT_CALL(manager().external_delegate(), OnPopupHidden());
  client().popup_controller(manager()).DoHide();
}

TEST_F(AutofillPopupControllerImplTest, DontHideWhenWaitingForData) {
  EXPECT_CALL(client().popup_view(), Hide).Times(0);
  client().popup_controller(manager()).PinView();

  // Hide() will not work for stale data or when focusing native UI.
  client().popup_controller(manager()).DoHide(PopupHidingReason::kStaleData);
  client().popup_controller(manager()).DoHide(PopupHidingReason::kEndEditing);

  // Check the expectations now since TearDown will perform a successful hide.
  Mock::VerifyAndClearExpectations(&manager().external_delegate());
  Mock::VerifyAndClearExpectations(&client().popup_view());
}

TEST_F(AutofillPopupControllerImplTest, ShouldReportHidingPopupReason) {
  base::HistogramTester histogram_tester;
  client().popup_controller(manager()).DoHide(PopupHidingReason::kTabGone);
  histogram_tester.ExpectTotalCount("Autofill.PopupHidingReason", 1);
  histogram_tester.ExpectBucketCount("Autofill.PopupHidingReason",
                                     /*kTabGone=*/8, 1);
}

// This is a regression test for crbug.com/521133 to ensure that we don't crash
// when suggestions updates race with user selections.
TEST_F(AutofillPopupControllerImplTest, SelectInvalidSuggestion) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});

  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(0);

  // The following should not crash:
  client().popup_controller(manager()).AcceptSuggestion(
      /*index=*/1, base::TimeTicks::Now());  // Out of bounds!
}

TEST_F(AutofillPopupControllerImplTest, AcceptSuggestionRespectsTimeout) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});

  // Calls before the threshold are ignored.
  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(0);
  client().popup_controller(manager()).AcceptSuggestion(0,
                                                        base::TimeTicks::Now());
  task_environment()->FastForwardBy(base::Milliseconds(100));
  client().popup_controller(manager()).AcceptSuggestion(/*index=*/0,
                                                        base::TimeTicks::Now());

  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion);
  task_environment()->FastForwardBy(base::Milliseconds(400));
  client().popup_controller(manager()).AcceptSuggestion(/*index=*/0,
                                                        base::TimeTicks::Now());

  histogram_tester.ExpectTotalCount(
      "Autofill.Popup.AcceptanceDelayThresholdNotMet", 2);
}

TEST_F(AutofillPopupControllerImplTest,
       AcceptSuggestionTimeoutIsUpdatedOnPopupMove) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});

  // Calls before the threshold are ignored.
  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(0);
  client().popup_controller(manager()).AcceptSuggestion(/*index=*/0,
                                                        base::TimeTicks::Now());
  task_environment()->FastForwardBy(base::Milliseconds(100));
  client().popup_controller(manager()).AcceptSuggestion(/*index=*/0,
                                                        base::TimeTicks::Now());

  histogram_tester.ExpectTotalCount(
      "Autofill.Popup.AcceptanceDelayThresholdNotMet", 2);
  task_environment()->FastForwardBy(base::Milliseconds(400));
  // Show the suggestions again (simulating, e.g., a click somewhere slightly
  // different).
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});

  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(0);
  client().popup_controller(manager()).AcceptSuggestion(/*index=*/0,
                                                        base::TimeTicks::Now());
  histogram_tester.ExpectTotalCount(
      "Autofill.Popup.AcceptanceDelayThresholdNotMet", 3);

  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion);
  // After waiting, suggestions are accepted again.
  task_environment()->FastForwardBy(base::Milliseconds(500));
  client().popup_controller(manager()).AcceptSuggestion(/*index=*/0,
                                                        base::TimeTicks::Now());
  histogram_tester.ExpectTotalCount(
      "Autofill.Popup.AcceptanceDelayThresholdNotMet", 3);
}

// Tests that when a picture-in-picture window is initialized, there is a call
// to the popup view to check if the autofill popup bounds overlap with the
// picture-in-picture window.
TEST_F(AutofillPopupControllerImplTest,
       CheckBoundsOverlapWithPictureInPicture) {
  client().popup_controller(manager());  // Creates the controller.
  EXPECT_CALL(client().popup_view(), OverlapsWithPictureInPictureWindow)
      .Times(1);
  PictureInPictureWindowManager* picture_in_picture_window_manager =
      PictureInPictureWindowManager::GetInstance();
  picture_in_picture_window_manager->EnterVideoPictureInPicture(web_contents());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(AutofillPopupControllerImplTest,
       AcceptPwdSuggestionInvokesWarningAndroid) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  ShowSuggestions(manager(), {PopupItemId::kPasswordEntry});

  // Calls are accepted immediately.
  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(1);
  EXPECT_CALL(client().show_pwd_migration_warning_callback(),
              Run(_, _,
                  password_manager::metrics_util::
                      PasswordMigrationWarningTriggers::kKeyboardAcessoryBar));
  client().popup_controller(manager()).AcceptSuggestion(
      0, base::TimeTicks::Now() + base::Milliseconds(500));
}

TEST_F(AutofillPopupControllerImplTest,
       AcceptUsernameSuggestionInvokesWarningAndroid) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  ShowSuggestions(manager(), {PopupItemId::kUsernameEntry});

  // Calls are accepted immediately.
  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(1);
  EXPECT_CALL(client().show_pwd_migration_warning_callback(), Run);
  client().popup_controller(manager()).AcceptSuggestion(
      0, base::TimeTicks::Now() + base::Milliseconds(500));
}

TEST_F(AutofillPopupControllerImplTest,
       AcceptPwdSuggestionNoWarningIfDisabledAndroid) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  ShowSuggestions(manager(), {PopupItemId::kPasswordEntry});

  // Calls are accepted immediately.
  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(1);
  EXPECT_CALL(client().show_pwd_migration_warning_callback(), Run).Times(0);
  client().popup_controller(manager()).AcceptSuggestion(
      0, base::TimeTicks::Now() + base::Milliseconds(500));
}

TEST_F(AutofillPopupControllerImplTest, AcceptAddressNoPwdWarningAndroid) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});

  // Calls are accepted immediately.
  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(1);
  EXPECT_CALL(client().show_pwd_migration_warning_callback(), Run).Times(0);
  client().popup_controller(manager()).AcceptSuggestion(
      0, base::TimeTicks::Now() + base::Milliseconds(500));
}
#endif

#if !BUILDFLAG(IS_ANDROID)
TEST_F(AutofillPopupControllerImplTest, SubPopupIsCreatedWithViewFromParent) {
  base::WeakPtr<AutofillPopupController> sub_controller =
      client().popup_controller(manager()).OpenSubPopup(
          {0, 0, 10, 10}, {}, AutoselectFirstSuggestion(false));
  EXPECT_TRUE(sub_controller);
}

TEST_F(AutofillPopupControllerImplTest,
       DelegateMethodsAreCalledOnlyByRootPopup) {
  EXPECT_CALL(manager().external_delegate(), OnPopupShown()).Times(0);
  base::WeakPtr<AutofillPopupController> sub_controller =
      client().popup_controller(manager()).OpenSubPopup(
          {0, 0, 10, 10}, {}, AutoselectFirstSuggestion(false));

  EXPECT_CALL(manager().external_delegate(), OnPopupHidden()).Times(0);
  sub_controller->Hide(PopupHidingReason::kUserAborted);

  EXPECT_CALL(manager().external_delegate(), OnPopupHidden());
  client().popup_controller(manager()).Hide(PopupHidingReason::kUserAborted);
}

TEST_F(AutofillPopupControllerImplTest, EventsAreDelegatedToChildrenAndView) {
  EXPECT_CALL(manager().external_delegate(), OnPopupShown()).Times(0);
  base::WeakPtr<AutofillPopupController> sub_controller =
      client().popup_controller(manager()).OpenSubPopup(
          {0, 0, 10, 10}, {}, AutoselectFirstSuggestion(false));

  content::NativeWebKeyboardEvent event = CreateKeyPressEvent(ui::VKEY_LEFT);
  EXPECT_CALL(client().sub_popup_view(), HandleKeyPressEvent)
      .WillOnce(Return(true));
  EXPECT_CALL(client().popup_view(), HandleKeyPressEvent).Times(0);
  EXPECT_TRUE(client().popup_controller(manager()).HandleKeyPressEvent(event));

  EXPECT_CALL(client().sub_popup_view(), HandleKeyPressEvent)
      .WillOnce(Return(false));
  EXPECT_CALL(client().popup_view(), HandleKeyPressEvent).Times(1);
  EXPECT_FALSE(client().popup_controller(manager()).HandleKeyPressEvent(event));
}

// Tests that the controller forwards calls to perform a button action (such as
// clicking a close button on a suggestion) to its delegate.
TEST_F(AutofillPopupControllerImplTest, ButtonActionsAreSentToDelegate) {
  ShowSuggestions(manager(), {PopupItemId::kCompose});
  EXPECT_CALL(manager().external_delegate(),
              DidPerformButtonActionForSuggestion);
  client().popup_controller(manager()).PerformButtonActionForSuggestion(0);
}
#endif

// Tests that the popup controller queries the view for its screen location.
TEST_F(AutofillPopupControllerImplTest, GetPopupScreenLocationCallsView) {
  ShowSuggestions(manager(), {PopupItemId::kCompose});

  using PopupScreenLocation = AutofillClient::PopupScreenLocation;
  constexpr gfx::Rect kSampleRect = gfx::Rect(123, 234);
  EXPECT_CALL(client().popup_view(), GetPopupScreenLocation)
      .WillOnce(Return(PopupScreenLocation{.bounds = kSampleRect}));
  EXPECT_THAT(client().popup_controller(manager()).GetPopupScreenLocation(),
              Optional(Field(&PopupScreenLocation::bounds, kSampleRect)));
}

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

class AutofillPopupControllerImplTestAccessibility
    : public AutofillPopupControllerImplTest {
 public:
  static constexpr int kAxUniqueId = 123;

  AutofillPopupControllerImplTestAccessibility()
      : accessibility_mode_override_(ui::AXMode::kScreenReader) {}
  AutofillPopupControllerImplTestAccessibility(
      AutofillPopupControllerImplTestAccessibility&) = delete;
  AutofillPopupControllerImplTestAccessibility& operator=(
      AutofillPopupControllerImplTestAccessibility&) = delete;
  ~AutofillPopupControllerImplTestAccessibility() override = default;

  void SetUp() override {
    AutofillPopupControllerImplTest::SetUp();

    ON_CALL(driver(), GetAxTreeId()).WillByDefault(Return(test_tree_id_));
    ON_CALL(client().popup_controller(manager()),
            GetRootAXPlatformNodeForWebContents)
        .WillByDefault(Return(&mock_ax_platform_node_));
    ON_CALL(mock_ax_platform_node_, GetDelegate)
        .WillByDefault(Return(&mock_ax_platform_node_delegate_));
    ON_CALL(client().popup_view(), GetAxUniqueId)
        .WillByDefault(Return(absl::optional<int32_t>(kAxUniqueId)));
    ON_CALL(mock_ax_platform_node_delegate_, GetFromTreeIDAndNodeID)
        .WillByDefault(Return(&mock_ax_platform_node_));
  }

  void TearDown() override {
    // This needs to bo reset explicit because having the mode set to
    // `kScreenReader` causes mocked functions to get called  with
    // `mock_ax_platform_node_delegate` after it has been destroyed.
    accessibility_mode_override_.ResetMode();
    AutofillPopupControllerImplTest::TearDown();
  }

 protected:
  content::ScopedAccessibilityModeOverride accessibility_mode_override_;
  MockAxPlatformNodeDelegate mock_ax_platform_node_delegate_;
  MockAxPlatformNode mock_ax_platform_node_;
  ui::AXTreeID test_tree_id_ = ui::AXTreeID::CreateNewAXTreeID();
};

// Test for successfully firing controls changed event for popup show/hide.
TEST_F(AutofillPopupControllerImplTestAccessibility,
       FireControlsChangedEventDuringShowAndHide) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  // Manually fire the event for popup show since setting the test view results
  // in the fire controls changed event not being sent.
  client().popup_controller(manager()).FireControlsChangedEvent(true);
  EXPECT_EQ(kAxUniqueId, ui::GetActivePopupAxUniqueId());

  client().popup_controller(manager()).DoHide();
  EXPECT_EQ(absl::nullopt, ui::GetActivePopupAxUniqueId());
}

// Test for attempting to fire controls changed event when ax tree manager
// fails to retrieve the ax platform node associated with the popup.
// No event is fired and global active popup ax unique id is not set.
TEST_F(AutofillPopupControllerImplTestAccessibility,
       FireControlsChangedEventNoAxPlatformNode) {
  EXPECT_CALL(mock_ax_platform_node_delegate_, GetFromTreeIDAndNodeID)
      .WillOnce(Return(nullptr));

  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  // Manually fire the event for popup show since setting the test view results
  // in the fire controls changed event not being sent.
  client().popup_controller(manager()).FireControlsChangedEvent(true);
  EXPECT_EQ(absl::nullopt, ui::GetActivePopupAxUniqueId());
}

// Test for attempting to fire controls changed event when failing to retrieve
// the autofill popup's ax unique id. No event is fired and the global active
// popup ax unique id is not set.
TEST_F(AutofillPopupControllerImplTestAccessibility,
       FireControlsChangedEventNoPopupAxUniqueId) {
  EXPECT_CALL(client().popup_view(), GetAxUniqueId)
      .WillOnce(testing::Return(absl::nullopt));

  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  // Manually fire the event for popup show since setting the test view results
  // in the fire controls changed event not being sent.
  client().popup_controller(manager()).FireControlsChangedEvent(true);
  EXPECT_EQ(absl::nullopt, ui::GetActivePopupAxUniqueId());
}
#endif

class AutofillPopupControllerImplTestHidingLogic
    : public AutofillPopupControllerImplTest {
 public:
  void SetUp() override {
    AutofillPopupControllerImplTest::SetUp();
    sub_frame_ = CreateAndNavigateChildFrame(
                     main_frame(), GURL("https://bar.com"), "sub_frame")
                     ->GetWeakDocumentPtr();
  }

  void TearDown() override {
    AutofillPopupControllerImplTest::TearDown();
  }

  BrowserAutofillManagerWithMockDelegate& sub_manager() {
    return manager(sub_frame());
  }

  content::RenderFrameHost* sub_frame() {
    return sub_frame_.AsRenderFrameHostIfValid();
  }

 private:
  content::WeakDocumentPtr sub_frame_;
};

// Tests that if the popup is shown in the *main frame*, destruction of the
// *sub frame* does not hide the popup.
TEST_F(AutofillPopupControllerImplTestHidingLogic,
       KeepOpenInMainFrameOnSubFrameDestruction) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(client().popup_controller(manager()), Hide).Times(0);
  content::RenderFrameHostTester::For(sub_frame())->Detach();
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&client().popup_controller(manager()));
}

// Tests that if the popup is shown in the *main frame*, a navigation in the
// *sub frame* does not hide the popup.
TEST_F(AutofillPopupControllerImplTestHidingLogic,
       KeepOpenInMainFrameOnSubFrameNavigation) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(client().popup_controller(manager()), Hide).Times(0);
  NavigateAndCommitFrame(sub_frame(), GURL("https://bar.com/"));
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&client().popup_controller(manager()));
}

// Tests that if the popup is shown in the *main frame*, destruction of the
// *main frame* hides the popup.
TEST_F(AutofillPopupControllerImplTestHidingLogic,
       HideInMainFrameOnDestruction) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kRendererEvent));
}

// Tests that if the popup is shown in the *sub frame*, destruction of the
// *sub frame* hides the popup.
TEST_F(AutofillPopupControllerImplTestHidingLogic,
       HideInSubFrameOnDestruction) {
  ShowSuggestions(sub_manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&sub_manager().external_delegate());
  EXPECT_CALL(client().popup_controller(sub_manager()),
              Hide(PopupHidingReason::kRendererEvent));
}

// Tests that if the popup is shown in the *main frame*, a navigation in the
// *main frame* hides the popup.
TEST_F(AutofillPopupControllerImplTestHidingLogic,
       HideInMainFrameOnMainFrameNavigation) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kNavigation));
  NavigateAndCommitFrame(main_frame(), GURL("https://bar.com/"));
}

// Tests that if the popup is shown in the *sub frame*, a navigation in the
// *sub frame* hides the popup.
TEST_F(AutofillPopupControllerImplTestHidingLogic,
       HideInSubFrameOnSubFrameNavigation) {
  ShowSuggestions(sub_manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&sub_manager().external_delegate());
  EXPECT_CALL(client().popup_controller(sub_manager()),
              Hide(PopupHidingReason::kNavigation));
  if (sub_frame()->ShouldChangeRenderFrameHostOnSameSiteNavigation()) {
    // If the RenderFrameHost changes, a RenderFrameDeleted will fire after
    // navigation, also triggering a `Hide()` call.
    EXPECT_CALL(client().popup_controller(sub_manager()),
                Hide(PopupHidingReason::kRendererEvent));
  }
  NavigateAndCommitFrame(sub_frame(), GURL("https://bar.com/"));
}

// Tests that if the popup is shown in the *sub frame*, a navigation in the
// *main frame* hides the popup.
TEST_F(AutofillPopupControllerImplTestHidingLogic,
       HideInSubFrameOnMainFrameNavigation) {
  ShowSuggestions(sub_manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&sub_manager().external_delegate());
  EXPECT_CALL(client().popup_controller(sub_manager()),
              Hide(PopupHidingReason::kRendererEvent));
  NavigateAndCommitFrame(main_frame(), GURL("https://bar.com/"));
}

}  // namespace autofill
