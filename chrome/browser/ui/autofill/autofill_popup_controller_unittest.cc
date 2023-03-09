// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
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
#include "components/autofill/content/browser/content_autofill_driver_test_api.h"
#include "components/autofill/content/browser/content_autofill_router.h"
#include "components/autofill/content/browser/content_autofill_router_test_api.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
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
#include "content/public/browser/browser_accessibility_state.h"
#endif

using base::ASCIIToUTF16;
using base::WeakPtr;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::StrictMock;

namespace autofill {
namespace {

ContentAutofillDriverTestApi test_api(ContentAutofillDriver* cadf) {
  return ContentAutofillDriverTestApi(cadf);
}

ContentAutofillRouterTestApi test_api(ContentAutofillRouter* cadf) {
  return ContentAutofillRouterTestApi(cadf);
}

ContentAutofillDriverFactoryTestApi test_api(
    ContentAutofillDriverFactory* cadf) {
  return ContentAutofillDriverFactoryTestApi(cadf);
}

class MockAutofillDriver : public ContentAutofillDriver {
 public:
  MockAutofillDriver(content::RenderFrameHost* rfh,
                     ContentAutofillRouter* router)
      : ContentAutofillDriver(rfh, router) {}

  MockAutofillDriver(MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(MockAutofillDriver&) = delete;

  ~MockAutofillDriver() override = default;
  MOCK_CONST_METHOD0(GetAxTreeId, ui::AXTreeID());
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
  MockAutofillExternalDelegate(BrowserAutofillManager* autofill_manager,
                               AutofillDriver* autofill_driver)
      : AutofillExternalDelegate(autofill_manager, autofill_driver) {}
  ~MockAutofillExternalDelegate() override = default;

  void DidSelectSuggestion(const std::u16string& value,
                           int frontend_id,
                           const Suggestion::BackendId& backend_id) override {}
  bool RemoveSuggestion(const std::u16string& value, int frontend_id) override {
    return true;
  }
  base::WeakPtr<AutofillExternalDelegate> GetWeakPtr() {
    return AutofillExternalDelegate::GetWeakPtr();
  }

  MOCK_METHOD(void, ClearPreviewedForm, (), (override));
  MOCK_METHOD(void, OnPopupSuppressed, (), (override));
  MOCK_METHOD(void, DidAcceptSuggestion, (const Suggestion&, int), (override));
};

class MockAutofillPopupView : public AutofillPopupView {
 public:
  MockAutofillPopupView() = default;
  MockAutofillPopupView(MockAutofillPopupView&) = delete;
  MockAutofillPopupView& operator=(MockAutofillPopupView&) = delete;
  ~MockAutofillPopupView() override = default;

  MOCK_METHOD(void, Show, (AutoselectFirstSuggestion), (override));
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(bool,
              HandleKeyPressEvent,
              (const content::NativeWebKeyboardEvent&),
              (override));
  MOCK_METHOD(void, OnSuggestionsChanged, (), (override));
  MOCK_METHOD(absl::optional<int32_t>, GetAxUniqueId, (), (override));
  MOCK_METHOD(void, AxAnnounce, (const std::u16string&), (override));

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
      const gfx::RectF& element_bounds)
      : AutofillPopupControllerImpl(external_delegate,
                                    nullptr,
                                    nullptr,
                                    element_bounds,
                                    base::i18n::UNKNOWN_DIRECTION) {}
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

class MockAxTreeManager : public ui::AXTreeManager {
 public:
  MockAxTreeManager() = default;
  MockAxTreeManager(MockAxTreeManager&) = delete;
  MockAxTreeManager& operator=(MockAxTreeManager&) = delete;
  ~MockAxTreeManager() override = default;

  MOCK_CONST_METHOD2(GetNodeFromTree,
                     ui::AXNode*(const ui::AXTreeID& tree_id,
                                 const int32_t node_id));
  MOCK_CONST_METHOD2(GetDelegate,
                     ui::AXPlatformNodeDelegate*(const ui::AXTreeID tree_id,
                                                 const int32_t node_id));
  MOCK_CONST_METHOD1(GetRootDelegate,
                     ui::AXPlatformNodeDelegate*(const ui::AXTreeID tree_id));
  MOCK_CONST_METHOD0(GetTreeID, ui::AXTreeID());
  MOCK_CONST_METHOD0(GetParentTreeID, ui::AXTreeID());
  MOCK_CONST_METHOD0(GetRootAsAXNode, ui::AXNode*());
  MOCK_CONST_METHOD0(GetParentNodeFromParentTree, ui::AXNode*());
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

  MOCK_CONST_METHOD0(GetDelegate, ui::AXPlatformNodeDelegate*());
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
    external_delegate_ = CreateExternalDelegate();
    autofill_popup_view_ = std::make_unique<NiceMock<MockAutofillPopupView>>();
    autofill_popup_controller_ = new NiceMock<TestAutofillPopupController>(
        external_delegate_->GetWeakPtr(), gfx::RectF());
    autofill_popup_controller_->SetViewForTesting(
        autofill_popup_view()->GetWeakPtr());
  }

  void TearDown() override {
    // This will make sure the controller and the view (if any) are both
    // cleaned up.
    if (autofill_popup_controller_) {
      autofill_popup_controller_->DoHide();
    }

    external_delegate_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  virtual std::unique_ptr<NiceMock<MockAutofillExternalDelegate>>
  CreateExternalDelegate() {
    // Fake that |driver| has queried a form.
    test_api(&test_api(autofill_driver()).autofill_router())
        .set_last_queried_source(autofill_driver());
    return std::make_unique<NiceMock<MockAutofillExternalDelegate>>(
        autofill_manager(), autofill_driver());
  }

  // Shows empty suggestions with the frontend ids passed as `ids`.
  void ShowSuggestions(const std::vector<int>& ids) {
    std::vector<Suggestion> suggestions;
    suggestions.reserve(ids.size());
    for (int id : ids) {
      suggestions.emplace_back("", "", "", id);
    }
    popup_controller().Show(std::move(suggestions),
                            AutoselectFirstSuggestion(false));
  }

  TestAutofillPopupController& popup_controller() {
    return *autofill_popup_controller_;
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

  ContentAutofillRouter* autofill_router() {
    return &test_api(autofill_client()->GetAutofillDriverFactory()).router();
  }

  NiceMock<MockAutofillDriver>* autofill_driver() {
    return autofill_driver_injector_[web_contents()];
  }

  BrowserAutofillManager* autofill_manager() {
    return static_cast<BrowserAutofillManager*>(
        autofill_driver()->autofill_manager());
  }

  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  TestAutofillDriverInjector<NiceMock<MockAutofillDriver>>
      autofill_driver_injector_;

  test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::unique_ptr<NiceMock<MockAutofillExternalDelegate>> external_delegate_;
  std::unique_ptr<NiceMock<MockAutofillPopupView>> autofill_popup_view_;
  raw_ptr<NiceMock<TestAutofillPopupController>> autofill_popup_controller_ =
      nullptr;
};

#if !BUILDFLAG(IS_CHROMEOS_ASH)
class AutofillPopupControllerAccessibilityUnitTest
    : public AutofillPopupControllerUnitTest {
 public:
  AutofillPopupControllerAccessibilityUnitTest()
      : accessibility_mode_setter_(ui::AXMode::kScreenReader) {}
  AutofillPopupControllerAccessibilityUnitTest(
      AutofillPopupControllerAccessibilityUnitTest&) = delete;
  AutofillPopupControllerAccessibilityUnitTest& operator=(
      AutofillPopupControllerAccessibilityUnitTest&) = delete;
  ~AutofillPopupControllerAccessibilityUnitTest() override = default;

  std::unique_ptr<NiceMock<MockAutofillExternalDelegate>>
  CreateExternalDelegate() override {
    // Fake that |driver| has queried a form.
    test_api(autofill_router()).set_last_queried_source(autofill_driver());
    return std::make_unique<NiceMock<MockAutofillExternalDelegate>>(
        autofill_manager(), autofill_driver());
  }

 protected:
  MockBrowserAutofillManager* autofill_manager() {
    return autofill_manager_injector_[web_contents()];
  }

  content::testing::ScopedContentAXModeSetter accessibility_mode_setter_;
  TestAutofillManagerInjector<MockBrowserAutofillManager>
      autofill_manager_injector_;
};
#endif

TEST_F(AutofillPopupControllerUnitTest, RemoveSuggestion) {
  ShowSuggestions({1, 1, POPUP_ITEM_ID_AUTOFILL_OPTIONS});

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

TEST_F(AutofillPopupControllerUnitTest, UpdateDataListValues) {
  ShowSuggestions({1});

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
  EXPECT_EQ(POPUP_ITEM_ID_DATALIST_ENTRY, result0.frontend_id);

  Suggestion result1 = popup_controller().GetSuggestionAt(1);
  EXPECT_EQ(std::u16string(), result1.main_text.value);
  EXPECT_TRUE(result1.labels.empty());
  EXPECT_EQ(std::u16string(), result1.additional_label);
  EXPECT_EQ(POPUP_ITEM_ID_SEPARATOR, result1.frontend_id);

  Suggestion result2 = popup_controller().GetSuggestionAt(2);
  EXPECT_EQ(std::u16string(), result2.main_text.value);
  EXPECT_TRUE(result2.labels.empty());
  EXPECT_EQ(std::u16string(), result2.additional_label);
  EXPECT_EQ(1, result2.frontend_id);

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
  EXPECT_EQ(POPUP_ITEM_ID_SEPARATOR,
            popup_controller().GetSuggestionAt(2).frontend_id);

  // Clear all data list values.
  data_list_values.clear();
  popup_controller().UpdateDataListValues(data_list_values, data_list_labels);

  ASSERT_EQ(1, popup_controller().GetLineCount());
  EXPECT_EQ(1, popup_controller().GetSuggestionAt(0).frontend_id);
}

TEST_F(AutofillPopupControllerUnitTest, PopupsWithOnlyDataLists) {
  // Create the popup with a single datalist element.
  ShowSuggestions({POPUP_ITEM_ID_DATALIST_ENTRY});

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
  EXPECT_EQ(POPUP_ITEM_ID_DATALIST_ENTRY,
            popup_controller().GetSuggestionAt(0).frontend_id);

  // Clear datalist values and check that the popup becomes hidden.
  EXPECT_CALL(popup_controller(), Hide(PopupHidingReason::kNoSuggestions));
  data_list_values.clear();
  popup_controller().UpdateDataListValues(data_list_values, data_list_values);
}

TEST_F(AutofillPopupControllerUnitTest, GetOrCreate) {
  NiceMock<MockAutofillExternalDelegate> delegate(autofill_manager(),
                                                  autofill_driver());

  WeakPtr<AutofillPopupControllerImpl> controller =
      AutofillPopupControllerImpl::GetOrCreate(
          WeakPtr<AutofillPopupControllerImpl>(), delegate.GetWeakPtr(),
          nullptr, nullptr, gfx::RectF(), base::i18n::UNKNOWN_DIRECTION);
  EXPECT_TRUE(controller.get());

  controller->Hide(PopupHidingReason::kViewDestroyed);

  controller = AutofillPopupControllerImpl::GetOrCreate(
      WeakPtr<AutofillPopupControllerImpl>(), delegate.GetWeakPtr(), nullptr,
      nullptr, gfx::RectF(), base::i18n::UNKNOWN_DIRECTION);
  EXPECT_TRUE(controller.get());

  WeakPtr<AutofillPopupControllerImpl> controller2 =
      AutofillPopupControllerImpl::GetOrCreate(
          controller, delegate.GetWeakPtr(), nullptr, nullptr, gfx::RectF(),
          base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(controller.get(), controller2.get());
  controller->Hide(PopupHidingReason::kViewDestroyed);

  NiceMock<TestAutofillPopupController>* test_controller =
      new NiceMock<TestAutofillPopupController>(delegate.GetWeakPtr(),
                                                gfx::RectF());
  EXPECT_CALL(*test_controller, Hide(PopupHidingReason::kViewDestroyed));

  gfx::RectF bounds(0.f, 0.f, 1.f, 2.f);
  base::WeakPtr<AutofillPopupControllerImpl> controller3 =
      AutofillPopupControllerImpl::GetOrCreate(
          test_controller->GetWeakPtr(), delegate.GetWeakPtr(), nullptr,
          nullptr, bounds, base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(bounds, static_cast<AutofillPopupController*>(controller3.get())
                        ->element_bounds());
  controller3->Hide(PopupHidingReason::kViewDestroyed);

  // Hide the test_controller to delete it.
  test_controller->DoHide();

  test_controller = new NiceMock<TestAutofillPopupController>(
      delegate.GetWeakPtr(), gfx::RectF());
  EXPECT_CALL(*test_controller, Hide).Times(0);

  const base::WeakPtr<AutofillPopupControllerImpl> controller4 =
      AutofillPopupControllerImpl::GetOrCreate(
          test_controller->GetWeakPtr(), delegate.GetWeakPtr(), nullptr,
          nullptr, bounds, base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(bounds,
            static_cast<const AutofillPopupController*>(controller4.get())
                ->element_bounds());
  delete test_controller;
}

TEST_F(AutofillPopupControllerUnitTest, ProperlyResetController) {
  ShowSuggestions(
      {POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY, POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY});

  // Now show a new popup with the same controller, but with fewer items.
  WeakPtr<AutofillPopupControllerImpl> controller =
      AutofillPopupControllerImpl::GetOrCreate(
          popup_controller().GetWeakPtr(), delegate()->GetWeakPtr(), nullptr,
          nullptr, gfx::RectF(), base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(0, controller->GetLineCount());
}

TEST_F(AutofillPopupControllerUnitTest, HidingClearsPreview) {
  // Create a new controller, because hiding destroys it and we can't destroy it
  // twice.
  StrictMock<MockAutofillExternalDelegate> delegate(autofill_manager(),
                                                    autofill_driver());
  StrictMock<TestAutofillPopupController>* test_controller =
      new StrictMock<TestAutofillPopupController>(delegate.GetWeakPtr(),
                                                  gfx::RectF());

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
  NiceMock<MockAutofillExternalDelegate> delegate(autofill_manager(),
                                                  autofill_driver());
  NiceMock<TestAutofillPopupController>* test_controller =
      new NiceMock<TestAutofillPopupController>(delegate.GetWeakPtr(),
                                                gfx::RectF());
  base::HistogramTester histogram_tester;
  // DoHide() invokes Hide() that also deletes the object itself.
  test_controller->DoHide(PopupHidingReason::kTabGone);

  histogram_tester.ExpectTotalCount("Autofill.PopupHidingReason", 1);
  histogram_tester.ExpectBucketCount("Autofill.PopupHidingReason",
                                     /*kTabGone=*/8, 1);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(AutofillPopupControllerAccessibilityUnitTest, FireControlsChangedEvent) {
  StrictMock<MockAxPlatformNodeDelegate> mock_ax_platform_node_delegate;
  StrictMock<MockAxPlatformNode> mock_ax_platform_node;
  const ui::AXTreeID& test_tree_id = ui::AXTreeID::CreateNewAXTreeID();

  // Test for successfully firing controls changed event for popup show/hide.
  {
    EXPECT_CALL(*autofill_driver(), GetAxTreeId())
        .Times(2)
        .WillRepeatedly(testing::Return(test_tree_id));
    EXPECT_CALL(*autofill_popup_view_, GetAxUniqueId)
        .Times(2)
        .WillRepeatedly(testing::Return(absl::optional<int32_t>(123)));
    EXPECT_CALL(popup_controller(), GetRootAXPlatformNodeForWebContents)
        .WillRepeatedly(testing::Return(&mock_ax_platform_node));
    EXPECT_CALL(mock_ax_platform_node, GetDelegate)
        .WillRepeatedly(testing::Return(&mock_ax_platform_node_delegate));
    EXPECT_CALL(mock_ax_platform_node_delegate, GetFromTreeIDAndNodeID)
        .Times(2)
        .WillRepeatedly(testing::Return(&mock_ax_platform_node));

    // Fire event for popup show and active popup ax unique id is set.
    popup_controller().FireControlsChangedEvent(true);
    EXPECT_EQ(123, ui::GetActivePopupAxUniqueId());

    // Fire event for popup hide and active popup ax unique id is cleared.
    popup_controller().FireControlsChangedEvent(false);
    EXPECT_EQ(absl::nullopt, ui::GetActivePopupAxUniqueId());
  }

  // Test for attempting to fire controls changed event when ax tree manager
  // fails to retrieve the ax platform node associated with the popup.
  // No event is fired and global active popup ax unique id is not set.
  {
    EXPECT_CALL(*autofill_driver(), GetAxTreeId())
        .WillOnce(testing::Return(test_tree_id));
    EXPECT_CALL(*autofill_popup_view_, GetAxUniqueId)
        .WillOnce(testing::Return(absl::optional<int32_t>(123)));
    EXPECT_CALL(popup_controller(), GetRootAXPlatformNodeForWebContents)
        .WillOnce(testing::Return(&mock_ax_platform_node));
    EXPECT_CALL(mock_ax_platform_node, GetDelegate)
        .WillOnce(testing::Return(&mock_ax_platform_node_delegate));
    EXPECT_CALL(mock_ax_platform_node_delegate, GetFromTreeIDAndNodeID)
        .WillOnce(testing::Return(nullptr));

    // No controls changed event is fired and active popup ax unique id is not
    // set.
    popup_controller().FireControlsChangedEvent(true);
    EXPECT_EQ(absl::nullopt, ui::GetActivePopupAxUniqueId());
  }

  // Test for attempting to fire controls changed event when failing to retrieve
  // the ax platform node associated with the popup.
  // No event is fired and global active popup ax unique id is not set.
  {
    EXPECT_CALL(popup_controller(), GetRootAXPlatformNodeForWebContents)
        .WillRepeatedly(testing::Return(&mock_ax_platform_node));
    EXPECT_CALL(*autofill_driver(), GetAxTreeId())
        .WillOnce(testing::Return(test_tree_id));
    EXPECT_CALL(mock_ax_platform_node, GetDelegate)
        .WillRepeatedly(testing::Return(&mock_ax_platform_node_delegate));
    EXPECT_CALL(mock_ax_platform_node_delegate, GetFromTreeIDAndNodeID)
        .WillOnce(testing::Return(nullptr));
    EXPECT_CALL(*autofill_popup_view_, GetAxUniqueId)
        .WillOnce(testing::Return(absl::optional<int32_t>(123)));

    // No controls changed event is fired and active popup ax unique id is not
    // set.
    popup_controller().FireControlsChangedEvent(true);
    EXPECT_EQ(absl::nullopt, ui::GetActivePopupAxUniqueId());
  }

  // Test for attempting to fire controls changed event when failing to retrieve
  // the autofill popup's ax unique id.
  // No event is fired and global active popup ax unique id is not set.
  {
    EXPECT_CALL(*autofill_driver(), GetAxTreeId())
        .WillOnce(testing::Return(test_tree_id));
    EXPECT_CALL(popup_controller(), GetRootAXPlatformNodeForWebContents)
        .WillRepeatedly(testing::Return(&mock_ax_platform_node));
    EXPECT_CALL(mock_ax_platform_node, GetDelegate)
        .WillRepeatedly(testing::Return(&mock_ax_platform_node_delegate));
    EXPECT_CALL(mock_ax_platform_node_delegate, GetFromTreeIDAndNodeID)
        .WillOnce(testing::Return(&mock_ax_platform_node));
    EXPECT_CALL(*autofill_popup_view_, GetAxUniqueId)
        .WillOnce(testing::Return(absl::nullopt));

    // No controls changed event is fired and active popup ax unique id is not
    // set.
    popup_controller().FireControlsChangedEvent(true);
    EXPECT_EQ(absl::nullopt, ui::GetActivePopupAxUniqueId());
  }
  // This needs to happen before TearDown() because having the mode set to
  // kScreenReader causes mocked functions to get called  with
  // mock_ax_platform_node_delegate after it has been destroyed.
  accessibility_mode_setter_.ResetMode();
}
#endif

// This is a regression test for crbug.com/521133 to ensure that we don't crash
// when suggestions updates race with user selections.
TEST_F(AutofillPopupControllerUnitTest, SelectInvalidSuggestion) {
  ShowSuggestions({1});

  EXPECT_CALL(*delegate(), DidAcceptSuggestion).Times(0);

  // The following should not crash:
  popup_controller().AcceptSuggestion(1);  // Out of bounds!
}

TEST_F(AutofillPopupControllerUnitTest, AcceptSuggestionRespectsTimeout) {
  ShowSuggestions({1});

  // Calls before the threshold are ignored.
  EXPECT_CALL(*delegate(), DidAcceptSuggestion).Times(0);
  popup_controller().AcceptSuggestion(0);
  task_environment()->FastForwardBy(base::Milliseconds(100));
  popup_controller().AcceptSuggestion(0);

  EXPECT_CALL(*delegate(), DidAcceptSuggestion);
  task_environment()->FastForwardBy(base::Milliseconds(400));
  popup_controller().AcceptSuggestion(0);
}

TEST_F(AutofillPopupControllerUnitTest, AcceptSuggestionWithoutThreshold) {
  ShowSuggestions({1});

  // Calls are accepted immediately.
  EXPECT_CALL(*delegate(), DidAcceptSuggestion).Times(1);
  popup_controller().AcceptSuggestionWithoutThreshold(0);
}

TEST_F(AutofillPopupControllerUnitTest,
       AcceptSuggestionTimeoutIsUpdatedOnPopupMove) {
  ShowSuggestions({1});

  // Calls before the threshold are ignored.
  EXPECT_CALL(*delegate(), DidAcceptSuggestion).Times(0);
  popup_controller().AcceptSuggestion(0);
  task_environment()->FastForwardBy(base::Milliseconds(100));
  popup_controller().AcceptSuggestion(0);

  task_environment()->FastForwardBy(base::Milliseconds(400));
  // Show the suggestions again (simulating, e.g., a click somewhere slightly
  // different).
  ShowSuggestions({1});

  EXPECT_CALL(*delegate(), DidAcceptSuggestion).Times(0);
  popup_controller().AcceptSuggestion(0);

  EXPECT_CALL(*delegate(), DidAcceptSuggestion);
  // After waiting, suggestions are accepted again.
  task_environment()->FastForwardBy(base::Milliseconds(500));
  popup_controller().AcceptSuggestion(0);
}

}  // namespace autofill
