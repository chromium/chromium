// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/at_memory_suggestion_controller.h"

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_test_base.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
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
    return *suggestion_controller_;
  }

  base::WeakPtr<AtMemorySuggestionController> suggestion_controller() {
    return suggestion_controller_;
  }

  MOCK_METHOD(void,
              ShowAtMemoryBottomSheet,
              (base::span<const Suggestion>,
               base::WeakPtr<AutofillSuggestionDelegate>),
              (override));
  MOCK_METHOD(void, HideAtMemoryBottomSheet, (), (override));

 private:
  base::WeakPtr<AtMemorySuggestionController> suggestion_controller_;
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
        std::move(suggestions), AutofillSuggestionTriggerSource::kAtMemory,
        AutoselectFirstSuggestion(false),
        AutofillSuggestionsIgnoreFocusLoss(false));
  }
};

// Tests that the controller correctly shows suggestions via the Autofill
// client.
TEST_F(AtMemorySuggestionControllerTest, ShowSuggestions) {
  std::vector<Suggestion> suggestions = {
      Suggestion(u"test", SuggestionType::kAddressEntry)};

  EXPECT_CALL(client(),
              ShowAtMemoryBottomSheet(ElementsAreArray(suggestions), _));
  EXPECT_CALL(
      manager().external_delegate(),
      OnSuggestionsShown(ElementsAreArray(suggestions), Eq(std::nullopt)));

  ShowSuggestions(manager(), suggestions);
}

// Tests that the controller hides suggestions and notifies the delegate.
TEST_F(AtMemorySuggestionControllerTest, HideSuggestions) {
  std::vector<Suggestion> suggestions = {
      Suggestion(u"test", SuggestionType::kAddressEntry)};

  ShowSuggestions(manager(), suggestions);

  EXPECT_CALL(manager().external_delegate(),
              OnSuggestionsHidden(SuggestionHidingReason::kUserAborted));

  client().suggestion_controller(manager()).Hide(
      SuggestionHidingReason::kUserAborted);
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
  client().suggestion_controller(manager()).Hide(
      SuggestionHidingReason::kUserAborted);
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
          /*form_control_ax_id=*/0, AutofillSuggestionTriggerSource::kAtMemory);

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
          /*form_control_ax_id=*/0, AutofillSuggestionTriggerSource::kAtMemory);

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
          /*form_control_ax_id=*/0, AutofillSuggestionTriggerSource::kAtMemory);

  base::WeakPtr<AutofillSuggestionController> controller2_weak =
      AutofillSuggestionController::GetOrCreate(
          controller1_weak, manager1.external_delegate().GetWeakPtrForTest(),
          web_contents(),
          PopupControllerCommon(manager1.driver().GetFrameToken(), {},
                                base::i18n::UNKNOWN_DIRECTION),
          /*form_control_ax_id=*/0, AutofillSuggestionTriggerSource::kAtMemory);

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

TEST_F(AtMemorySuggestionControllerTest,
       HideCallsClientHideAtMemoryBottomSheet) {
  ShowSuggestions(manager(),
                  {Suggestion(u"test", SuggestionType::kAddressEntry)});
  EXPECT_CALL(client(), HideAtMemoryBottomSheet);
  client().suggestion_controller(manager()).Hide(
      SuggestionHidingReason::kViewDestroyed);
}

}  // namespace
}  // namespace autofill
