// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/at_memory_suggestion_controller.h"

#include <vector>

#include "chrome/browser/ui/android/autofill/at_memory_bottom_sheet_bridge.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_test_base.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/ui/mock_autofill_suggestion_delegate.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Eq;

class MockAtMemoryBottomSheetBridge : public AtMemoryBottomSheetBridge {
 public:
  explicit MockAtMemoryBottomSheetBridge(
      AtMemorySuggestionController* controller)
      : AtMemoryBottomSheetBridge(controller) {}
  ~MockAtMemoryBottomSheetBridge() override = default;

  MOCK_METHOD(void,
              RequestShowContent,
              (base::span<const Suggestion>),
              (override));
};

class TestAtMemorySuggestionControllerAutofillClient
    : public TestContentAutofillClient {
 public:
  using TestContentAutofillClient::TestContentAutofillClient;

  AtMemorySuggestionController& suggestion_controller(
      BrowserAutofillManagerForPopupTest& manager) {
    if (!suggestion_controller_) {
      auto* controller = new AtMemorySuggestionController(
          manager.external_delegate().GetWeakPtrForTest(), &GetWebContents(),
          PopupControllerCommon(manager.driver().GetFrameToken(), {},
                                base::i18n::UNKNOWN_DIRECTION));
      suggestion_controller_ = controller->GetWeakPtr();
    }
    EnsureMockBridge();
    return *suggestion_controller_;
  }

  void EnsureMockBridge() {
    if (suggestion_controller_ &&
        !suggestion_controller_->bridge_for_testing()) {
      auto mock_bridge = std::make_unique<MockAtMemoryBottomSheetBridge>(
          suggestion_controller_.get());
      mock_bridge_ = mock_bridge.get();
      suggestion_controller_->SetBridgeForTesting(std::move(mock_bridge));
    }
  }

  base::WeakPtr<AtMemorySuggestionController> suggestion_controller() {
    return suggestion_controller_;
  }

  MockAtMemoryBottomSheetBridge* mock_bridge() {
    EnsureMockBridge();
    return mock_bridge_;
  }

 private:
  base::WeakPtr<AtMemorySuggestionController> suggestion_controller_;
  raw_ptr<MockAtMemoryBottomSheetBridge> mock_bridge_ = nullptr;
};

class AtMemorySuggestionControllerTest
    : public AutofillSuggestionControllerTestBase<
          TestAtMemorySuggestionControllerAutofillClient> {
 protected:
  void TearDown() override {
    if (client().suggestion_controller()) {
      client().suggestion_controller()->Hide(
          SuggestionHidingReason::kViewDestroyed);
    }
    AutofillSuggestionControllerTestBase::TearDown();
  }

  void ShowSuggestions(Manager& manager, std::vector<Suggestion> suggestions) {
    FocusWebContentsOnFrame(
        static_cast<ContentAutofillDriver&>(manager.driver())
            .render_frame_host());
    client().suggestion_controller(manager).Show(
        AutofillSuggestionController::GenerateSuggestionUiSessionId(),
        std::move(suggestions),
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        AutoselectFirstSuggestion(false),
        AutofillSuggestionsIgnoreFocusLoss(false),
        /*search_bar_initial_value=*/{});
  }
};

// Tests that the controller correctly shows suggestions via its bridge.
TEST_F(AtMemorySuggestionControllerTest, ShowSuggestions) {
  std::vector<Suggestion> suggestions = {
      Suggestion(u"test", SuggestionType::kAddressEntry)};

  client().suggestion_controller(manager());
  EXPECT_CALL(*client().mock_bridge(),
              RequestShowContent(ElementsAreArray(suggestions)));
  EXPECT_CALL(manager().external_delegate(),
              OnSuggestionsShown(ElementsAreArray(suggestions), _));

  ShowSuggestions(manager(), suggestions);
}

// Tests that the controller dismisses the bridge and notifies the delegate.
TEST_F(AtMemorySuggestionControllerTest, HideSuggestions) {
  std::vector<Suggestion> suggestions = {
      Suggestion(u"test", SuggestionType::kAddressEntry)};

  ShowSuggestions(manager(), suggestions);

  EXPECT_CALL(manager().external_delegate(),
              OnSuggestionsHidden(SuggestionHidingReason::kUserAborted));

  AtMemorySuggestionController& controller =
      client().suggestion_controller(manager());
  controller.Hide(SuggestionHidingReason::kUserAborted);
}

// Tests that the controller ignores focus loss and end editing hiding reasons.
TEST_F(AtMemorySuggestionControllerTest, IgnoreFocusLossAndEndEditing) {
  std::vector<Suggestion> suggestions = {
      Suggestion(u"test", SuggestionType::kAddressEntry)};

  ShowSuggestions(manager(), suggestions);

  EXPECT_CALL(manager().external_delegate(), OnSuggestionsHidden).Times(0);

  client().suggestion_controller(manager()).Hide(
      SuggestionHidingReason::kEndEditing);
  client().suggestion_controller(manager()).Hide(
      SuggestionHidingReason::kFocusChanged);

  testing::Mock::VerifyAndClearExpectations(&manager().external_delegate());

  EXPECT_CALL(manager().external_delegate(),
              OnSuggestionsHidden(SuggestionHidingReason::kUserAborted));
  AtMemorySuggestionController& controller =
      client().suggestion_controller(manager());
  controller.Hide(SuggestionHidingReason::kUserAborted);
}

// Tests that a new controller is created if the delegate changes (e.g.
// navigation to a subframe).
TEST_F(AtMemorySuggestionControllerTest, RecreatesControllerIfDelegateChanges) {
  Manager& manager1 = manager();
  base::WeakPtr<AutofillSuggestionController> controller1_weak =
      AutofillSuggestionController::GetOrCreate(
          /*previous=*/nullptr,
          manager1.external_delegate().GetWeakPtrForTest(), web_contents(),
          PopupControllerCommon(manager1.driver().GetFrameToken(), {},
                                base::i18n::UNKNOWN_DIRECTION),
          /*form_control_ax_id=*/0,
          AutofillSuggestionTriggerSource::kAtMemoryTriggerString);

  content::RenderFrameHost* subframe = CreateAndNavigateChildFrame(
      main_frame(), GURL("https://bar.com/"), "subframe");
  Manager& manager2 = manager(subframe);
  ASSERT_NE(&manager1.external_delegate(), &manager2.external_delegate());

  EXPECT_CALL(manager1.external_delegate(),
              OnSuggestionsHidden(SuggestionHidingReason::kViewDestroyed));

  base::WeakPtr<AutofillSuggestionController> controller2_weak =
      AutofillSuggestionController::GetOrCreate(
          controller1_weak, manager2.external_delegate().GetWeakPtrForTest(),
          web_contents(),
          PopupControllerCommon(manager2.driver().GetFrameToken(), {},
                                base::i18n::UNKNOWN_DIRECTION),
          /*form_control_ax_id=*/0,
          AutofillSuggestionTriggerSource::kAtMemoryTriggerString);

  EXPECT_NE(controller1_weak.get(), controller2_weak.get());

  if (controller2_weak) {
    controller2_weak->Hide(SuggestionHidingReason::kViewDestroyed);
  }
}

// Tests that the existing controller is reused if the delegate is the same.
TEST_F(AtMemorySuggestionControllerTest, RecyclesControllerIfDelegateIsSame) {
  Manager& manager1 = manager();
  base::WeakPtr<AutofillSuggestionController> controller1_weak =
      AutofillSuggestionController::GetOrCreate(
          /*previous=*/nullptr,
          manager1.external_delegate().GetWeakPtrForTest(), web_contents(),
          PopupControllerCommon(manager1.driver().GetFrameToken(), {},
                                base::i18n::UNKNOWN_DIRECTION),
          /*form_control_ax_id=*/0,
          AutofillSuggestionTriggerSource::kAtMemoryTriggerString);

  base::WeakPtr<AutofillSuggestionController> controller2_weak =
      AutofillSuggestionController::GetOrCreate(
          controller1_weak, manager1.external_delegate().GetWeakPtrForTest(),
          web_contents(),
          PopupControllerCommon(manager1.driver().GetFrameToken(), {},
                                base::i18n::UNKNOWN_DIRECTION),
          /*form_control_ax_id=*/0,
          AutofillSuggestionTriggerSource::kAtMemoryTriggerString);

  EXPECT_EQ(controller1_weak.get(), controller2_weak.get());

  if (controller2_weak) {
    controller2_weak->Hide(SuggestionHidingReason::kViewDestroyed);
  }
}

// Tests that accepting a suggestion notifies the delegate.
TEST_F(AtMemorySuggestionControllerTest, AcceptSuggestion) {
  std::vector<Suggestion> suggestions = {
      Suggestion(u"test", SuggestionType::kAddressEntry)};
  ShowSuggestions(manager(), suggestions);

  EXPECT_CALL(
      manager().external_delegate(),
      DidAcceptSuggestion(
          suggestions[0],
          testing::Field(
              &AutofillSuggestionDelegate::SuggestionMetadata::multi_index,
              std::vector<size_t>{0})));

  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kTap);
}

// Tests that AtMemoryBottomSheetBridge methods correctly route to the
// controller and delegate.
TEST_F(AtMemorySuggestionControllerTest, DelegateRouting) {
  testing::NiceMock<MockAutofillSuggestionDelegate> mock_delegate;
  auto* controller = new AtMemorySuggestionController(
      mock_delegate.GetWeakPtr(), web_contents(),
      PopupControllerCommon({}, gfx::RectF(), base::i18n::UNKNOWN_DIRECTION));

  auto mock_bridge =
      std::make_unique<MockAtMemoryBottomSheetBridge>(controller);
  MockAtMemoryBottomSheetBridge* bridge_ptr = mock_bridge.get();
  controller->SetBridgeForTesting(std::move(mock_bridge));

  Suggestion child(u"child", SuggestionType::kAddressEntry);
  Suggestion parent(u"parent", SuggestionType::kAddressEntry);
  parent.children = {child};
  std::vector<Suggestion> suggestions = {parent};
  controller->Show(
      AutofillSuggestionController::GenerateSuggestionUiSessionId(),
      suggestions, AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
      AutoselectFirstSuggestion(false),
      AutofillSuggestionsIgnoreFocusLoss(false),
      /*search_bar_initial_value=*/{});

  // OnQuerySubmitted routes to OnSearchSubmitted.
  EXPECT_CALL(mock_delegate, OnSearchSubmitted(std::u16string(u"query")));
  controller->OnQuerySubmitted(u"query");

  // OnQueryTextChanged routes to OnFilterChanged.
  EXPECT_CALL(mock_delegate, OnFilterChanged(std::u16string(u"query")));
  controller->OnQueryTextChanged(u"query");

  // OnSuggestionSelected routes to DidAcceptSuggestion.
  EXPECT_CALL(
      mock_delegate,
      DidAcceptSuggestion(
          parent,
          testing::Field(
              &AutofillSuggestionDelegate::SuggestionMetadata::multi_index,
              std::vector<size_t>{0})));
  controller->OnSuggestionSelected(0);

  // OnChildSuggestionsShown routes to OnSuggestionsShown with parent metadata.
  EXPECT_CALL(mock_delegate,
              OnSuggestionsShown(ElementsAreArray(parent.children), _));
  controller->OnChildSuggestionsShown(0);

  // OnChildSuggestionSelected routes to DidAcceptSuggestion for child.
  EXPECT_CALL(
      mock_delegate,
      DidAcceptSuggestion(
          child,
          testing::Field(
              &AutofillSuggestionDelegate::SuggestionMetadata::multi_index,
              std::vector<size_t>{0, 0})));
  controller->OnChildSuggestionSelected(0, 0);

  // IsSearching routes to IsSearching on delegate.
  EXPECT_CALL(mock_delegate, IsSearching).WillOnce(testing::Return(true));
  EXPECT_TRUE(controller->IsSearching());

  // OnSuggestionDismissed calls RemoveSuggestion, erases item, and re-shows
  // content.
  EXPECT_CALL(mock_delegate, RemoveSuggestion(parent))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*bridge_ptr, RequestShowContent(testing::ElementsAre()));
  controller->OnSuggestionDismissed(0);
  EXPECT_TRUE(controller->GetSuggestions().empty());

  controller->OnDismissed();
}

}  // namespace
}  // namespace autofill
