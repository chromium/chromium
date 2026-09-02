// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"

#include <optional>

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl_test_api.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_test_base.h"
#include "chrome/browser/ui/autofill/test_autofill_popup_controller_autofill_client.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/ui/popup_interaction.h"
#include "components/autofill/core/common/aliases.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_active_popup.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/gfx/range/range.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "content/public/test/scoped_accessibility_mode_override.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)
namespace autofill {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Matcher;
using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Return;

using SingleEntryRemovalMethod = AutofillMetrics::SingleEntryRemovalMethod;

Matcher<const AutofillSuggestionDelegate::SuggestionMetadata&>
EqualsSuggestionMetadata(
    AutofillSuggestionDelegate::SuggestionMetadata metadata) {
  return AllOf(
      Field(&AutofillSuggestionDelegate::SuggestionMetadata::multi_index,
            metadata.multi_index),
      Field(&AutofillSuggestionDelegate::SuggestionMetadata::from_search_result,
            metadata.from_search_result));
}

class AutofillPopupControllerImplTest
    : public AutofillSuggestionControllerTestBase<
          TestAutofillPopupControllerAutofillClient<>> {
 public:
  AutofillPopupControllerImplTest() {
    feature_list_.InitWithFeatures(
        {features::kAutofillAtMemory,
         features::debug::kAtMemorySkipEnablementChecks},
        {});
  }

  // Encapsulates the setup required to get the controller and its associated
  // AtMemoryController into a search-ready state for AtMemory tests.
  void ShowAtMemoryPopup() {
    // 1. Set the trigger source inside the delegate.
    manager().external_delegate().OnQuery(
        FormData(), FormFieldData(), gfx::Rect(),
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString);

    // 2. Setup the bridge so the mock delegate executes real initialization
    // logic.
    EXPECT_CALL(manager().external_delegate(), OnSuggestionsShown)
        .WillOnce([&](base::span<const Suggestion> suggestions,
                      base::optional_ref<
                          const AutofillSuggestionDelegate::SuggestionMetadata>
                          parent_suggestion_metadata) {
          manager()
              .external_delegate()
              .AutofillExternalDelegate::OnSuggestionsShown(
                  suggestions, parent_suggestion_metadata);
        });

    // 3. Actually show the suggestions, which triggers the search session
    // initialization in AtMemoryController.
    ShowSuggestions(manager(), {SuggestionType::kAtMemorySearchResult},
                    AutofillSuggestionTriggerSource::kAtMemoryTriggerString);
  }

  // Simulates a user typing a query into the AtMemory search bar and explicitly
  // submitting the search (by accepting the search affordance), mocking the
  // backend response and updating the UI state.
  void SimulateAtMemoryQuery(const std::u16string& query,
                             const std::vector<std::u16string>& results) {
    // 1. Prepare the backend mock results.
    std::vector<MemorySearchResult> entries;
    for (const auto& value : results) {
      entries.emplace_back(MemoryDataType::kNameFull, u"Name", value);
    }
    MemorySearchResults search_results(
        MemorySearchStatus::kFinalResponseSuccess, std::move(entries));

    // 2. Setup the backend expectation if the query is non-empty.
    if (!query.empty()) {
      EXPECT_CALL(*client().at_memory_query_service(),
                  Query(std::u16string_view(query), _, _, _))
          .WillOnce(base::test::RunOnceCallback<3>(std::move(search_results)));
    }

    // 3. Trigger the search via the UI.
    // First, simulate the user typing the query, which updates the input
    // filter.
    client().suggestion_controller(manager()).SetFilter(
        AutofillPopupController::StringFilter(query),
        AutofillPopupController::FilterSource::kInputChanged);
    // Explicitly submit the search (simulating hitting the Enter key in the
    // search bar).
    if (!query.empty()) {
      client().suggestion_controller(manager()).SetFilter(
          AutofillPopupController::StringFilter(query),
          AutofillPopupController::FilterSource::kSearchSubmitted);
    }

    // 4. Manually update the controller's suggestions to reflect the mock
    // results. This bypasses the full async callback chain to keep the test
    // focused.
    std::vector<Suggestion> suggestions;
    for (const auto& value : results) {
      suggestions.emplace_back(value, SuggestionType::kAtMemorySearchResult);
    }
    test_api(static_cast<AutofillPopupControllerImpl&>(
                 client().suggestion_controller(manager())))
        .SetSuggestions(std::move(suggestions));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AutofillPopupControllerImplTest, AcceptSuggestionRespectsTimeout) {
  // Calls before the threshold are ignored.
  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call);
    EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion);
  }

  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  client().suggestion_controller(manager()).OnPopupPainted();
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);
  task_environment()->FastForwardBy(base::Milliseconds(100));
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);
  task_environment()->FastForwardBy(base::Milliseconds(400));

  // Only now suggestions should be accepted.
  check.Call();
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);
}

// Tests that the time threshold for accepting suggestions only starts counting
// once the view is painted.
TEST_F(AutofillPopupControllerImplTest, AcceptSuggestionRespectsWaitsForPaint) {
  // Calls before the threshold are ignored.
  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call);
    EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion);
  }

  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});

  // No matter how long painting takes, the threshold starts counting only once
  // the popup has been painted.
  task_environment()->FastForwardBy(base::Seconds(2));
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);

  client().suggestion_controller(manager()).OnPopupPainted();
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);
  task_environment()->FastForwardBy(base::Milliseconds(500));

  // Only now suggestions should be accepted.
  check.Call();
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);
}

// Tests that reshowing the suggestions resets the accept threshold.
TEST_F(AutofillPopupControllerImplTest,
       AcceptSuggestionTimeoutIsUpdatedOnPopupUpdate) {
  // Calls before the threshold are ignored.
  MockFunction<void()> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call);
    EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion);
  }

  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});

  client().suggestion_controller(manager()).OnPopupPainted();
  // Calls before the threshold are ignored.
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);
  task_environment()->FastForwardBy(base::Milliseconds(100));
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);
  task_environment()->FastForwardBy(base::Milliseconds(400));

  // Show the suggestions again (simulating, e.g., a click somewhere slightly
  // different).
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});

  // The threshold timer does not start until the popup is painted.
  task_environment()->FastForwardBy(base::Seconds(2));
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);
  client().suggestion_controller(manager()).OnPopupPainted();

  // After waiting again, suggestions become acceptable.
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);
  task_environment()->FastForwardBy(base::Milliseconds(500));
  check.Call();
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);
}

TEST_F(AutofillPopupControllerImplTest, SubPopupIsCreatedWithViewFromParent) {
  base::WeakPtr<AutofillSuggestionController> sub_controller =
      client().suggestion_controller(manager()).OpenSubPopup(
          {0, 0, 10, 10}, {}, AutoselectFirstSuggestion(false));
  EXPECT_TRUE(sub_controller);
}

// Tests that a sub-popup shares its UI session id with its parent controller.
TEST_F(AutofillPopupControllerImplTest, SubPopupHasSameUiSessionIdAsParent) {
  const std::optional<AutofillSuggestionController::UiSessionId> parent_id =
      client().suggestion_controller(manager()).GetUiSessionId();
  ASSERT_TRUE(parent_id.has_value());
  base::WeakPtr<AutofillSuggestionController> sub_controller =
      client().suggestion_controller(manager()).OpenSubPopup(
          {0, 0, 10, 10}, {}, AutoselectFirstSuggestion(false));
  EXPECT_TRUE(sub_controller);
  EXPECT_EQ(sub_controller->GetUiSessionId(), parent_id);
}

TEST_F(AutofillPopupControllerImplTest,
       PopupInteraction_SubPopupMetricsAreLogged) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  ON_CALL(*client().sub_popup_view(), Show).WillByDefault(Return(true));

  base::WeakPtr<AutofillSuggestionController> sub_controller =
      client().suggestion_controller(manager()).OpenSubPopup(
          {0, 0, 10, 10},
          {Suggestion(SuggestionType::kUndo),
           Suggestion(SuggestionType::kAddressEntry)},
          AutoselectFirstSuggestion(false));
  ASSERT_TRUE(sub_controller);
  static_cast<AutofillPopupController&>(*sub_controller).OnPopupPainted();
  histogram_tester.ExpectBucketCount(
      "Autofill.PopupInteraction.PopupLevel.1.Address",
      PopupInteraction::kPopupShown, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.PopupInteraction.PopupLevel.1.Address", 1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PopupInteraction_PopupLevel_1_SuggestionShown"));

  static_cast<AutofillPopupController&>(*sub_controller)
      .SelectSuggestion(/*index=*/0);
  histogram_tester.ExpectBucketCount(
      "Autofill.PopupInteraction.PopupLevel.1.Address",
      PopupInteraction::kSuggestionSelected, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.PopupInteraction.PopupLevel.1.Address", 2);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                "Autofill_PopupInteraction_PopupLevel_1_SuggestionSelected"));

  task_environment()->FastForwardBy(base::Milliseconds(1000));
  sub_controller->AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);

  histogram_tester.ExpectBucketCount(
      "Autofill.PopupInteraction.PopupLevel.1.Address",
      PopupInteraction::kSuggestionAccepted, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.PopupInteraction.PopupLevel.1.Address", 3);
  histogram_tester.ExpectTotalCount("Autofill.PopupInteraction.PopupLevel.1",
                                    3);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                "Autofill_PopupInteraction_PopupLevel_1_SuggestionAccepted"));
}

TEST_F(AutofillPopupControllerImplTest,
       PopupInteraction_NonAddressSuggestion_LogOnlyHistogramMetrics) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  ON_CALL(*client().popup_view(), Show).WillByDefault(Return(true));

  ShowSuggestions(manager(), {SuggestionType::kAutocompleteEntry});

  histogram_tester.ExpectBucketCount(
      "Autofill.PopupInteraction.PopupLevel.0.Autocomplete",
      PopupInteraction::kPopupShown, 1);
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Autofill_PopupInteraction_PopupLevel_0_SuggestionShown"));
}

TEST_F(
    AutofillPopupControllerImplTest,
    PopupInteraction_TriggerSourcesThatOpensThePopupIndirectly_SubPopupMetricsAreNotLogged) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  ON_CALL(*client().popup_view(), Show).WillByDefault(Return(true));

  auto assert_popup_interaction_metrics_are_empty = [&]() {
    histogram_tester.ExpectBucketCount(
        "Autofill.PopupInteraction.PopupLevel.0.Address",
        PopupInteraction::kPopupShown, 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.PopupInteraction.PopupLevel.0.Address", 0);
    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "Autofill_PopupInteraction_PopupLevel_0_SuggestionShown"));
  };

  ShowSuggestions(manager(), {SuggestionType::kAddressEntry},
                  AutofillSuggestionTriggerSource::kTextFieldValueChanged);
  assert_popup_interaction_metrics_are_empty();

  ShowSuggestions(
      manager(), {SuggestionType::kAddressEntry},
      AutofillSuggestionTriggerSource::kComposeDelayedProactiveNudge);
  assert_popup_interaction_metrics_are_empty();
}

TEST_F(AutofillPopupControllerImplTest,
       PopupInteraction_RootPopupMetricsAreLogged) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  ON_CALL(*client().popup_view(), Show).WillByDefault(Return(true));

  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  histogram_tester.ExpectBucketCount(
      "Autofill.PopupInteraction.PopupLevel.0.Address",
      PopupInteraction::kPopupShown, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.PopupInteraction.PopupLevel.0.Address", 1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PopupInteraction_PopupLevel_0_SuggestionShown"));

  static_cast<AutofillPopupController&>(
      client().suggestion_controller(manager()))
      .SelectSuggestion(/*index=*/0);
  histogram_tester.ExpectBucketCount(
      "Autofill.PopupInteraction.PopupLevel.0.Address",
      PopupInteraction::kSuggestionSelected, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.PopupInteraction.PopupLevel.0.Address", 2);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                "Autofill_PopupInteraction_PopupLevel_0_SuggestionSelected"));

  client().suggestion_controller(manager()).OnPopupPainted();
  task_environment()->FastForwardBy(base::Milliseconds(1000));
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);

  histogram_tester.ExpectBucketCount(
      "Autofill.PopupInteraction.PopupLevel.0.Address",
      PopupInteraction::kSuggestionAccepted, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.PopupInteraction.PopupLevel.0.Address", 3);
  histogram_tester.ExpectTotalCount("Autofill.PopupInteraction.PopupLevel.0",
                                    3);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                "Autofill_PopupInteraction_PopupLevel_0_SuggestionAccepted"));
}

TEST_F(AutofillPopupControllerImplTest,
       OnSuggestionsHiddenIsCalledOnlyByRootPopup) {
  // `OnSuggestionsShown` is also called by sub-popups, but they pass non-empty
  // metadata.
  EXPECT_CALL(manager().external_delegate(),
              OnSuggestionsShown(_, Ne(std::nullopt)));
  ON_CALL(*client().sub_popup_view(), Show).WillByDefault(Return(true));
  base::WeakPtr<AutofillSuggestionController> sub_controller =
      client().suggestion_controller(manager()).OpenSubPopup(
          {0, 0, 10, 10}, {}, AutoselectFirstSuggestion(false));

  EXPECT_CALL(manager().external_delegate(), OnSuggestionsHidden).Times(0);
  sub_controller->Hide(SuggestionHidingReason::kUserAborted);

  EXPECT_CALL(manager().external_delegate(),
              OnSuggestionsHidden(SuggestionHidingReason::kUserAborted));
  client().suggestion_controller(manager()).Hide(
      SuggestionHidingReason::kUserAborted);
}

// Tests that the correct parent index is passed to the delegate for a level 1
// sub-popup.
TEST_F(AutofillPopupControllerImplTest,
       OnSuggestionsShownPassesCorrectIndicesForSubPopup_Level1) {
  // Set expectation on root view to return index 2 as the anchor.
  EXPECT_CALL(*client().popup_view(), GetIndexOfSubPopupAnchorSuggestion)
      .WillOnce(Return(2));

  EXPECT_CALL(
      manager().external_delegate(),
      OnSuggestionsShown(_, Eq(AutofillSuggestionDelegate::SuggestionMetadata(
                                {.multi_index = {2}}))));

  ON_CALL(*client().sub_popup_view(), Show).WillByDefault(Return(true));
  base::WeakPtr<AutofillSuggestionController> sub_controller =
      client().suggestion_controller(manager()).OpenSubPopup(
          {0, 0, 10, 10}, {}, AutoselectFirstSuggestion(false));
}

// Tests that the correct parent indices are recursively passed to the delegate
// for a level 2 sub-popup.
TEST_F(AutofillPopupControllerImplTest,
       OnSuggestionsShownPassesCorrectIndicesForSubPopup_Level2) {
  NiceMock<MockAutofillPopupView> sub2_popup_view;

  // Root view returns index 2 for sub1 anchor.
  EXPECT_CALL(*client().popup_view(), GetIndexOfSubPopupAnchorSuggestion)
      .WillRepeatedly(Return(2));

  // Sub1 view returns index 1 for sub2 anchor.
  EXPECT_CALL(*client().sub_popup_view(), GetIndexOfSubPopupAnchorSuggestion)
      .WillOnce(Return(1));

  // When sub1 opens sub2, it will call sub1_view->CreateSubPopupView.
  // We mock it to return sub2_popup_view.
  EXPECT_CALL(*client().sub_popup_view(), CreateSubPopupView)
      .WillOnce(Return(sub2_popup_view.GetWeakPtr()));

  {
    InSequence s;
    EXPECT_CALL(
        manager().external_delegate(),
        OnSuggestionsShown(_, Eq(AutofillSuggestionDelegate::SuggestionMetadata(
                                  {.multi_index = {2}}))));
    EXPECT_CALL(
        manager().external_delegate(),
        OnSuggestionsShown(_, Eq(AutofillSuggestionDelegate::SuggestionMetadata(
                                  {.multi_index = {2, 1}}))));
  }

  ON_CALL(*client().sub_popup_view(), Show).WillByDefault(Return(true));
  ON_CALL(sub2_popup_view, Show).WillByDefault(Return(true));

  base::WeakPtr<AutofillSuggestionController> sub1_controller =
      client().suggestion_controller(manager()).OpenSubPopup(
          {0, 0, 10, 10}, {}, AutoselectFirstSuggestion(false));

  base::WeakPtr<AutofillSuggestionController> sub2_controller =
      static_cast<AutofillPopupController*>(sub1_controller.get())
          ->OpenSubPopup({0, 0, 10, 10}, {}, AutoselectFirstSuggestion(false));
}

TEST_F(AutofillPopupControllerImplTest, EventsAreDelegatedToChildrenAndView) {
  EXPECT_CALL(manager().external_delegate(), OnSuggestionsShown).Times(0);
  base::WeakPtr<AutofillSuggestionController> sub_controller =
      client().suggestion_controller(manager()).OpenSubPopup(
          {0, 0, 10, 10}, {}, AutoselectFirstSuggestion(false));

  input::NativeWebKeyboardEvent event = CreateKeyPressEvent(ui::VKEY_LEFT);
  EXPECT_CALL(*client().sub_popup_view(), HandleKeyPressEvent)
      .WillOnce(Return(true));
  EXPECT_CALL(*client().popup_view(), HandleKeyPressEvent).Times(0);
  EXPECT_TRUE(
      client().suggestion_controller(manager()).HandleKeyPressEvent(event));

  EXPECT_CALL(*client().sub_popup_view(), HandleKeyPressEvent)
      .WillOnce(Return(false));
  EXPECT_CALL(*client().popup_view(), HandleKeyPressEvent).Times(1);
  EXPECT_FALSE(
      client().suggestion_controller(manager()).HandleKeyPressEvent(event));
}

// The second popup is also the second "sub_popup_level". This test asserts that
// the information regarding the popup level is passed on to the delegate.
TEST_F(AutofillPopupControllerImplTest, PopupForwardsSuggestionPosition) {
  base::WeakPtr<AutofillSuggestionController> sub_controller =
      client().suggestion_controller(manager()).OpenSubPopup(
          {0, 0, 10, 10}, {Suggestion(SuggestionType::kAddressEntry)},
          AutoselectFirstSuggestion(false));
  ASSERT_TRUE(sub_controller);
  test_api(static_cast<AutofillPopupControllerImpl&>(*sub_controller))
      .SetView(client().sub_popup_view()->GetWeakPtr());

  EXPECT_CALL(manager().external_delegate(),
              DidAcceptSuggestion(
                  _, EqualsSuggestionMetadata({.multi_index = {0, 0}})));

  task_environment()->FastForwardBy(base::Milliseconds(1000));
  sub_controller->AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);
}

// Tests that unacceptable suggestions cannot be accepted.
TEST_F(AutofillPopupControllerImplTest, DoesNotAcceptUnacceptableSuggestions) {
  Suggestion suggestion(u"Open the pod bay doors, HAL",
                        SuggestionType::kAutocompleteEntry);
  suggestion.acceptability =
      Suggestion::Acceptability::kSelectableButUnacceptable;
  ShowSuggestions(manager(), {std::move(suggestion)});

  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(1000));
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);
}

// Tests that unselectable suggestions cannot be selected.
TEST_F(AutofillPopupControllerImplTest, DoesNotSelectUnselectableSuggestions) {
  Suggestion suggestion(u"I'm sorry, Dave. I'm afraid I can't do that.",
                        SuggestionType::kAutocompleteEntry);
  suggestion.acceptability =
      Suggestion::Acceptability::kUnselectableAndUnacceptable;
  ShowSuggestions(manager(), {std::move(suggestion)});

  EXPECT_CALL(manager().external_delegate(), DidSelectSuggestion).Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(1000));
  client().suggestion_controller(manager()).SelectSuggestion(/*index=*/0);
}

// Tests that suggestions that are selectable but unacceptable can still be
// selected.
TEST_F(AutofillPopupControllerImplTest,
       SelectsSelectableButUnacceptableSuggestions) {
  Suggestion suggestion(u"Alright, Dave.", SuggestionType::kAutocompleteEntry);
  suggestion.acceptability =
      Suggestion::Acceptability::kSelectableButUnacceptable;
  ShowSuggestions(manager(), {std::move(suggestion)});

  EXPECT_CALL(manager().external_delegate(), DidSelectSuggestion);
  task_environment()->FastForwardBy(base::Milliseconds(1000));
  client().suggestion_controller(manager()).SelectSuggestion(/*index=*/0);
}

// Parameterized tests for AutofillSuggestionTriggerSource values that are
// exempt from standard safety checks.
class AutofillPopupControllerImplTestWithTriggerSource
    : public AutofillPopupControllerImplTest,
      public ::testing::WithParamInterface<AutofillSuggestionTriggerSource> {};

// Tests that the accidental click safety bounds checks are ignored.
TEST_P(AutofillPopupControllerImplTestWithTriggerSource,
       IgnoreClickOutsideCheck) {
  const AutofillSuggestionTriggerSource trigger_source = GetParam();
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry}, trigger_source);
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_TRUE(client()
                  .suggestion_controller(manager())
                  .ShouldIgnoreMouseObservedOutsideItemBoundsCheck());
}

// Tests that updates to the popup suggestions do not reset the accidental click
// lockout (idle barrier).
TEST_P(AutofillPopupControllerImplTestWithTriggerSource,
       UpdateDoesNotResetIdleBarrier) {
  const AutofillSuggestionTriggerSource trigger_source = GetParam();
  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion);

  ShowSuggestions(manager(), {SuggestionType::kAddressEntry}, trigger_source);
  client().suggestion_controller(manager()).OnPopupPainted();

  // Fast forward 400ms (barrier not expired yet).
  task_environment()->FastForwardBy(base::Milliseconds(400));

  // Reshow suggestions with the trigger source. This should NOT reset the
  // 500ms barrier.
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry}, trigger_source);

  // Fast forward another 150ms (total 550ms since initial open, 150ms since
  // reshow). The barrier should have expired.
  task_environment()->FastForwardBy(base::Milliseconds(150));

  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AutofillPopupControllerImplTestWithTriggerSource,
    ::testing::Values(
        AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
        AutofillSuggestionTriggerSource::kAtMemoryKeyboardShortcut,
        AutofillSuggestionTriggerSource::kAtMemoryContextMenu,
        AutofillSuggestionTriggerSource::kAtMemoryInactivityNudge));

// Tests that Compose saved state notification popup gets hidden after 2
// seconds, but not after 1 second.
TEST_F(AutofillPopupControllerImplTest,
       TimedHideComposeSavedStateNotification) {
  ShowSuggestions(manager(), {SuggestionType::kComposeSavedStateNotification});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  ::testing::MockFunction<void()> check;
  {
    ::testing::InSequence s;
    EXPECT_CALL(check, Call);
    EXPECT_CALL(client().suggestion_controller(manager()),
                Hide(SuggestionHidingReason::kFadeTimerExpired));
  }
  task_environment()->FastForwardBy(base::Seconds(1));
  check.Call();
  task_environment()->FastForwardBy(base::Seconds(1));
  Mock::VerifyAndClearExpectations(&client().suggestion_controller(manager()));
}

TEST_F(AutofillPopupControllerImplTest,
       PopupHidesOnWebContentsFocusLossIfViewIsNotFocused) {
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});

  EXPECT_CALL(*client().popup_view(), HasFocus).WillOnce(Return(false));
  EXPECT_CALL(*client().popup_view(), Hide);
  client().suggestion_controller(manager()).Hide(
      SuggestionHidingReason::kFocusChanged);

  Mock::VerifyAndClearExpectations(client().popup_view());
}

TEST_F(AutofillPopupControllerImplTest,
       PopupDoesntHideOnWebContentsFocusLossIfViewIsFocused) {
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});

  EXPECT_CALL(*client().popup_view(), HasFocus).WillOnce(Return(true));
  EXPECT_CALL(*client().popup_view(), Hide).Times(0);
  client().suggestion_controller(manager()).Hide(
      SuggestionHidingReason::kFocusChanged);

  Mock::VerifyAndClearExpectations(client().popup_view());
}

// Tests that focus loss does not hide the popup if the
// `AutofillSuggestionsIgnoreFocusLoss` parameter is set to `true`.
TEST_F(AutofillPopupControllerImplTest,
       PopupDoesNotHideOnFocusLossIfParameterIsSet) {
  ShowSuggestions(manager(), {SuggestionType::kFillAutofillAi},
                  AutofillSuggestionTriggerSource::kFormControlElementClicked,
                  AutofillSuggestionsIgnoreFocusLoss(true));

  ON_CALL(*client().popup_view(), HasFocus).WillByDefault(Return(false));
  EXPECT_CALL(*client().popup_view(), Hide).Times(0);
  client().suggestion_controller(manager()).Hide(
      SuggestionHidingReason::kFocusChanged);

  Mock::VerifyAndClearExpectations(client().popup_view());
}

TEST_F(AutofillPopupControllerImplTest,
       PopupDoesntHideOnEndEditingFromRendererIfViewIsFocused) {
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});

  EXPECT_CALL(*client().popup_view(), HasFocus).WillOnce(Return(true));
  EXPECT_CALL(*client().popup_view(), Hide).Times(0);
  client().suggestion_controller(manager()).Hide(
      SuggestionHidingReason::kEndEditing);

  Mock::VerifyAndClearExpectations(client().popup_view());
}

// Tests that a main frame resize event with an unchanged size does not hide the
// popup.
TEST_F(AutofillPopupControllerImplTest,
       PrimaryMainFrameResizeIgnoredWhenSizeUnchanged) {
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});

  AutofillPopupHideHelper* hide_helper =
      test_api(client().suggestion_controller(manager())).popup_hide_helper();
  ASSERT_TRUE(hide_helper);
  content::WebContentsObserver& observer = *hide_helper;

  EXPECT_CALL(*client().popup_view(), Hide).Times(0);
  observer.PrimaryMainFrameWasResized(/*width_changed=*/false);

  Mock::VerifyAndClearExpectations(client().popup_view());
}

// Tests that a main frame resize event with a changed size hides the popup.
TEST_F(AutofillPopupControllerImplTest,
       PrimaryMainFrameResizeHidesPopupWhenSizeChanged) {
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});

  AutofillPopupHideHelper* hide_helper =
      test_api(client().suggestion_controller(manager())).popup_hide_helper();
  ASSERT_TRUE(hide_helper);
  content::WebContentsObserver& observer = *hide_helper;

  const gfx::Size current_size = web_contents()->GetSize();
  web_contents()->Resize(
      gfx::Rect(current_size.width() + 10, current_size.height() + 10));

  EXPECT_CALL(*client().popup_view(), Hide);
  observer.PrimaryMainFrameWasResized(/*width_changed=*/true);

  Mock::VerifyAndClearExpectations(client().popup_view());
}

// Tests that calling Show() when the popup view has focus but the focused
// frame is null (e.g. because it was detached) does not cause a crash due to
// a null pointer dereference.
TEST_F(AutofillPopupControllerImplTest,
       ShowWithFocusedViewAndNullFocusedFrame_NoCrash) {
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});

  EXPECT_CALL(*client().popup_view(), HasFocus).WillRepeatedly(Return(true));

  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_frame())->AppendChild("child");
  FocusWebContentsOnFrame(child_rfh);
  content::RenderFrameHostTester::For(child_rfh)->Detach();
  ASSERT_EQ(web_contents()->GetFocusedFrame(), nullptr);

  // This should not crash.
  client().suggestion_controller(manager()).Show(
      AutofillSuggestionController::GenerateSuggestionUiSessionId(),
      {Suggestion(u"Search Query", SuggestionType::kAddressEntry)},
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      AutoselectFirstSuggestion(false),
      AutofillSuggestionsIgnoreFocusLoss(false),
      /*search_bar_initial_value=*/{});
}

TEST_F(AutofillPopupControllerImplTest,
       RemoveAutocompleteSuggestion_IgnoresClickOutsideCheck) {
  ShowSuggestions(manager(), {SuggestionType::kAutocompleteEntry,
                              SuggestionType::kAutocompleteEntry});

  // Generate a popup, so it can be hidden later. It doesn't matter what the
  // external_delegate thinks is being shown in the process, since we are just
  // testing the popup here.
  test::GenerateTestAutofillPopup(&manager().external_delegate());

  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(
                  Field(&Suggestion::type, SuggestionType::kAutocompleteEntry)))
      .WillOnce(Return(true));
  // Remove the first entry. The popup should be redrawn since its size has
  // changed.
  EXPECT_CALL(*client().popup_view(),
              OnSuggestionsChanged(/*prefer_prev_arrow_side=*/false));
  EXPECT_TRUE(client().suggestion_controller(manager()).RemoveSuggestion(
      0,
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  Mock::VerifyAndClearExpectations(client().popup_view());

  EXPECT_TRUE(client()
                  .suggestion_controller(manager())
                  .ShouldIgnoreMouseObservedOutsideItemBoundsCheck());
}

// Tests that if the popup is shown in the *main frame*, changing the zoom hides
// the popup.
TEST_F(AutofillPopupControllerImplTest, HideInMainFrameOnZoomChange) {
  zoom::ZoomController::CreateForWebContents(web_contents());
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  // Triggered by OnZoomChanged().
  EXPECT_CALL(client().suggestion_controller(manager()),
              Hide(SuggestionHidingReason::kContentAreaMoved));
  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents());
  zoom_controller->SetZoomLevel(zoom_controller->GetZoomLevel() + 1.0);
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&client().suggestion_controller(manager()));
}

TEST_F(AutofillPopupControllerImplTest,
       SuggestionFiltering_NoFilteringByDefault) {
  AutofillPopupController& controller =
      client().suggestion_controller(manager());
  ShowSuggestions(manager(),
                  {Suggestion(u"abc", SuggestionType::kAutocompleteEntry)});

  EXPECT_EQ(controller.GetSuggestions().size(), 1u);
  EXPECT_EQ(controller.GetSuggestionFilterMatches().size(), 0u);
}

TEST_F(AutofillPopupControllerImplTest,
       SuggestionFiltering_SuggestionChangeNotifications) {
  AutofillPopupController& controller =
      client().suggestion_controller(manager());
  ShowSuggestions(manager(),
                  {
                      Suggestion(u"abc", SuggestionType::kAutocompleteEntry),
                      Suggestion(u"axx", SuggestionType::kAutocompleteEntry),
                  });

  EXPECT_CALL(*client().popup_view(),
              OnSuggestionsChanged(/*prefer_prev_arrow_side=*/true));
  controller.SetFilter(AutofillPopupController::StringFilter(u"ab"),
                       AutofillPopupController::FilterSource::kInputChanged);

  EXPECT_CALL(*client().popup_view(),
              OnSuggestionsChanged(/*prefer_prev_arrow_side=*/true));
  controller.SetFilter(std::nullopt,
                       AutofillPopupController::FilterSource::kInputChanged);
}

TEST_F(AutofillPopupControllerImplTest,
       OnSuggestionsChanged_PreferPrevArrowSide_True) {
  ShowSuggestions(manager(), {SuggestionType::kCreditCardEntry});

  test_api(static_cast<AutofillPopupControllerImpl&>(
               client().suggestion_controller(manager())))
      .SetPreferPrevArrowSideOnSuggestionsUpdate(true);

  EXPECT_CALL(*client().popup_view(),
              OnSuggestionsChanged(/*prefer_prev_arrow_side=*/true));

  client().suggestion_controller(manager()).OnSuggestionsChanged();
}

TEST_F(AutofillPopupControllerImplTest,
       OnSuggestionsChanged_PreferPrevArrowSide_False) {
  ShowSuggestions(manager(), {SuggestionType::kCreditCardEntry});

  test_api(static_cast<AutofillPopupControllerImpl&>(
               client().suggestion_controller(manager())))
      .SetPreferPrevArrowSideOnSuggestionsUpdate(false);

  EXPECT_CALL(*client().popup_view(),
              OnSuggestionsChanged(/*prefer_prev_arrow_side=*/false));

  client().suggestion_controller(manager()).OnSuggestionsChanged();
}

TEST_F(AutofillPopupControllerImplTest, SuggestionFiltering_MatchingMainText) {
  AutofillPopupController& controller =
      client().suggestion_controller(manager());
  ShowSuggestions(manager(),
                  {
                      Suggestion(u"abc", SuggestionType::kAutocompleteEntry),
                      Suggestion(u"abx", SuggestionType::kAutocompleteEntry),
                      Suggestion(u"axx", SuggestionType::kAutocompleteEntry),
                  });

  EXPECT_EQ(controller.GetSuggestions().size(), 3u);
  EXPECT_EQ(controller.GetSuggestionFilterMatches().size(), 0u);

  controller.SetFilter(AutofillPopupController::StringFilter(u"Ab"),
                       AutofillPopupController::FilterSource::kInputChanged);
  EXPECT_EQ(controller.GetSuggestions().size(), 2u);
  EXPECT_EQ(controller.GetSuggestionFilterMatches().size(), 2u);
  EXPECT_THAT(controller.GetSuggestionFilterMatches(),
              ::testing::ElementsAre(
                  std::optional<AutofillPopupController::SuggestionFilterMatch>(
                      AutofillPopupController::SuggestionFilterMatch{
                          .main_text_match = gfx::Range(0, 2),
                      }),
                  std::optional<AutofillPopupController::SuggestionFilterMatch>(
                      AutofillPopupController::SuggestionFilterMatch{
                          .main_text_match = gfx::Range(0, 2),
                      })));

  controller.SetFilter(AutofillPopupController::StringFilter(u"abcdefg"),
                       AutofillPopupController::FilterSource::kInputChanged);
  EXPECT_EQ(controller.GetSuggestions().size(), 0u);
  EXPECT_EQ(controller.GetSuggestionFilterMatches().size(), 0u);

  controller.SetFilter(std::nullopt,
                       AutofillPopupController::FilterSource::kInputChanged);
  EXPECT_EQ(controller.GetSuggestions().size(), 3u);
  EXPECT_EQ(controller.GetSuggestionFilterMatches().size(), 0u);
}

// Tests that suggestion filter match has correct range when the suggestion
// contains characters that change their length after lowercasing.
TEST_F(AutofillPopupControllerImplTest,
       SuggestionFiltering_MatchingMainText_SuggestionLengthChangingCase) {
  AutofillPopupController& controller =
      client().suggestion_controller(manager());

  // "e\u0301" is "e" with a combining acute accent (length 2 in UTF-16).
  // Searching for "e" should match it and return length 2.
  ShowSuggestions(
      manager(), {
                     Suggestion(u"e\u0301", SuggestionType::kAutocompleteEntry),
                 });

  controller.SetFilter(AutofillPopupController::StringFilter(u"e"),
                       AutofillPopupController::FilterSource::kInputChanged);

  EXPECT_EQ(controller.GetSuggestions().size(), 1u);
  EXPECT_EQ(controller.GetSuggestionFilterMatches().size(), 1u);

  // The match range should be (0, 2) in "e\u0301" (length 2).
  EXPECT_THAT(controller.GetSuggestionFilterMatches(),
              ::testing::ElementsAre(
                  std::optional<AutofillPopupController::SuggestionFilterMatch>(
                      AutofillPopupController::SuggestionFilterMatch{
                          .main_text_match = gfx::Range(0, 2),
                      })));
}

TEST_F(AutofillPopupControllerImplTest,
       SuggestionFiltering_SuggestionIsDeletedFromFilteredList) {
  AutofillPopupController& controller =
      client().suggestion_controller(manager());
  ShowSuggestions(manager(),
                  {
                      Suggestion(u"abc", SuggestionType::kAutocompleteEntry),
                      Suggestion(u"abx", SuggestionType::kAutocompleteEntry),
                      Suggestion(u"axx", SuggestionType::kAutocompleteEntry),
                  });

  controller.SetFilter(AutofillPopupController::StringFilter(u"ab"),
                       AutofillPopupController::FilterSource::kInputChanged);
  EXPECT_EQ(controller.GetSuggestions().size(), 2u);

  EXPECT_CALL(manager().external_delegate(), RemoveSuggestion)
      .WillOnce(Return(true));
  controller.RemoveSuggestion(
      0, AutofillMetrics::SingleEntryRemovalMethod::kDeleteButtonClicked);
  EXPECT_EQ(controller.GetSuggestions().size(), 1u);
  EXPECT_EQ(controller.GetSuggestionFilterMatches().size(), 1u);

  controller.SetFilter(std::nullopt,
                       AutofillPopupController::FilterSource::kInputChanged);
  EXPECT_EQ(controller.GetSuggestions().size(), 2u);
  EXPECT_EQ(controller.GetSuggestionFilterMatches().size(), 0u);
}

TEST_F(AutofillPopupControllerImplTest,
       SuggestionFiltering_StaticSuggestionsAreNotFilteredOut) {
  using enum SuggestionType;

  Suggestion footer_suggestion1 = Suggestion(kSeparator);
  footer_suggestion1.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  Suggestion footer_suggestion2 = Suggestion(kUndo);
  footer_suggestion2.filtration_policy = Suggestion::FiltrationPolicy::kStatic;

  AutofillPopupController& controller =
      client().suggestion_controller(manager());
  ShowSuggestions(manager(), {
                                 Suggestion(u"abc", kAddressEntry),
                                 Suggestion(u"abx", kAddressEntry),
                                 std::move(footer_suggestion1),
                                 std::move(footer_suggestion2),
                             });

  controller.SetFilter(AutofillPopupController::StringFilter(u"ab"),
                       AutofillPopupController::FilterSource::kInputChanged);
  EXPECT_EQ(controller.GetSuggestions().size(), 4u);
  EXPECT_THAT(controller.GetSuggestions(),
              ElementsAre(Field(&Suggestion::type, kAddressEntry),
                          Field(&Suggestion::type, kAddressEntry),
                          Field(&Suggestion::type, kSeparator),
                          Field(&Suggestion::type, kUndo)));
  EXPECT_THAT(
      controller.GetSuggestionFilterMatches(),
      ElementsAre(std::optional<AutofillPopupController::SuggestionFilterMatch>(
                      AutofillPopupController::SuggestionFilterMatch{
                          .main_text_match = gfx::Range(0, 2),
                      }),
                  std::optional<AutofillPopupController::SuggestionFilterMatch>(
                      AutofillPopupController::SuggestionFilterMatch{
                          .main_text_match = gfx::Range(0, 2),
                      }),
                  std::nullopt, std::nullopt));

  controller.SetFilter(AutofillPopupController::StringFilter(u"abc"),
                       AutofillPopupController::FilterSource::kInputChanged);
  EXPECT_EQ(controller.GetSuggestions().size(), 3u);
  EXPECT_THAT(controller.GetSuggestions(),
              ElementsAre(Field(&Suggestion::type, kAddressEntry),
                          Field(&Suggestion::type, kSeparator),
                          Field(&Suggestion::type, kUndo)));
  EXPECT_THAT(
      controller.GetSuggestionFilterMatches(),
      ElementsAre(std::optional<AutofillPopupController::SuggestionFilterMatch>(
                      AutofillPopupController::SuggestionFilterMatch{
                          .main_text_match = gfx::Range(0, 3),
                      }),
                  std::nullopt, std::nullopt));

  controller.SetFilter(AutofillPopupController::StringFilter(u"abcdef"),
                       AutofillPopupController::FilterSource::kInputChanged);
  EXPECT_EQ(controller.GetSuggestions().size(), 2u);
  EXPECT_THAT(controller.GetSuggestions(),
              ElementsAre(Field(&Suggestion::type, kSeparator),
                          Field(&Suggestion::type, kUndo)));
  EXPECT_THAT(controller.GetSuggestionFilterMatches(),
              ElementsAre(std::nullopt, std::nullopt));
}

TEST_F(AutofillPopupControllerImplTest,
       SuggestionFiltering_SuggestionsAreFilteredByTabIndex) {
  Suggestion pay_later_tab_suggestion = Suggestion(SuggestionType::kBnplEntry);
  pay_later_tab_suggestion.tab_index = kPayLaterSuggestionTabIndex;
  Suggestion pay_later_tab_footer = Suggestion(SuggestionType::kBnplFootnote);
  pay_later_tab_footer.tab_index = kPayLaterSuggestionTabIndex;

  AutofillPopupController& controller =
      client().suggestion_controller(manager());
  ShowSuggestions(manager(), {
                                 Suggestion(SuggestionType::kCreditCardEntry),
                                 std::move(pay_later_tab_suggestion),
                                 std::move(pay_later_tab_footer),
                             });

  ASSERT_EQ(controller.GetSuggestions().size(), 3u);

  controller.SetFilter(kDefaultSuggestionTabIndex,
                       AutofillPopupController::FilterSource::kTabSelected);
  EXPECT_EQ(controller.GetSuggestions().size(), 1u);
  EXPECT_THAT(
      controller.GetSuggestions(),
      ElementsAre(Field(&Suggestion::type, SuggestionType::kCreditCardEntry)));
  EXPECT_THAT(controller.GetSuggestionFilterMatches(),
              ElementsAre(std::nullopt));

  controller.SetFilter(kPayLaterSuggestionTabIndex,
                       AutofillPopupController::FilterSource::kTabSelected);
  EXPECT_EQ(controller.GetSuggestions().size(), 2u);
  EXPECT_THAT(
      controller.GetSuggestions(),
      ElementsAre(Field(&Suggestion::type, SuggestionType::kBnplEntry),
                  Field(&Suggestion::type, SuggestionType::kBnplFootnote)));
  EXPECT_THAT(controller.GetSuggestionFilterMatches(),
              ElementsAre(std::nullopt, std::nullopt));
}

TEST_F(AutofillPopupControllerImplTest,
       ClearState_HidesAndClearsViewIfTabStateChanges) {
  ShowSuggestions(manager(), {SuggestionType::kCreditCardEntry});

  AutofillPopupController& controller =
      client().suggestion_controller(manager());
  EXPECT_TRUE(
      test_api(static_cast<AutofillPopupControllerImpl&>(controller)).view());

  // Calling ClearState with matching non-tabbed popup does not clear `view_`.
  test_api(static_cast<AutofillPopupControllerImpl&>(controller)).ClearState();
  EXPECT_TRUE(
      test_api(static_cast<AutofillPopupControllerImpl&>(controller)).view());

  // Calling ClearState with mismatching tabbed popup clears `view_`.
  test_api(static_cast<AutofillPopupControllerImpl&>(controller))
      .SetShowTabbedPopup(true);
  test_api(static_cast<AutofillPopupControllerImpl&>(controller)).ClearState();
  EXPECT_FALSE(
      test_api(static_cast<AutofillPopupControllerImpl&>(controller)).view());
}

TEST_F(AutofillPopupControllerImplTest,
       SuggestionFiltering_HasFilteredOutSuggestions) {
  using enum SuggestionType;

  AutofillPopupController& controller =
      client().suggestion_controller(manager());
  ShowSuggestions(manager(), {
                                 Suggestion(u"abcd", kAddressEntry),
                                 Suggestion(u"abxy", kAddressEntry),
                             });

  controller.SetFilter(AutofillPopupController::StringFilter(u"ab"),
                       AutofillPopupController::FilterSource::kInputChanged);
  EXPECT_FALSE(controller.HasFilteredOutSuggestions());

  controller.SetFilter(AutofillPopupController::StringFilter(u"abc"),
                       AutofillPopupController::FilterSource::kInputChanged);
  EXPECT_TRUE(controller.HasFilteredOutSuggestions());
}

TEST_F(AutofillPopupControllerImplTest,
       AtMemory_NoFilter_NoSuggestionsMessageNotShown) {
  ShowSuggestions(manager(), {SuggestionType::kAtMemorySearchResult},
                  AutofillSuggestionTriggerSource::kAtMemoryTriggerString);
  EXPECT_FALSE(
      client().suggestion_controller(manager()).ShouldShowNoSuggestionsMessage(
          AutofillPopupView::SearchBarConfig{
              .placeholder = u"Recall from memory",
              .initial_value = {},
              .no_results_message = u""}));
}

// Tests that the "no suggestions" message is not shown when AtMemory is
// triggered and the query returns results.
TEST_F(AutofillPopupControllerImplTest,
       AtMemory_FilterWithResults_NoSuggestionsMessageNotShown) {
  ShowAtMemoryPopup();
  SimulateAtMemoryQuery(/*query=*/u"res", /*results=*/{u"result"});
  EXPECT_FALSE(
      client().suggestion_controller(manager()).ShouldShowNoSuggestionsMessage(
          AutofillPopupView::SearchBarConfig{
              .placeholder = u"Recall from memory",
              .initial_value = {},
              .no_results_message = u""}));
}
// Tests that clearing the search query clears the suggestions in an AtMemory
// session.
TEST_F(AutofillPopupControllerImplTest, AtMemory_ClearingFilterClearsResults) {
  ShowAtMemoryPopup();
  AutofillPopupController& controller =
      client().suggestion_controller(manager());

  // 1. Simulate a search with results.
  SimulateAtMemoryQuery(/*query=*/u"res", /*results=*/{u"result"});
  ASSERT_EQ(controller.GetSuggestions().size(), 1u);

  // 2. Clear the filter.
  // The controller should notify the delegate even when the filter is nullopt.
  SimulateAtMemoryQuery(/*query=*/u"", /*results=*/{});

  // 3. Verify suggestions are now empty.
  EXPECT_EQ(controller.GetSuggestions().size(), 0u);
}

// Tests that the "no suggestions" message is not shown when AtMemory is
// triggered and the query returns no results.
TEST_F(AutofillPopupControllerImplTest,
       AtMemory_FilterWithNoResults_NoSuggestionsMessageNotShown) {
  ShowAtMemoryPopup();
  SimulateAtMemoryQuery(/*query=*/u"abc", /*results=*/{});
  EXPECT_FALSE(
      client().suggestion_controller(manager()).ShouldShowNoSuggestionsMessage(
          AutofillPopupView::SearchBarConfig{
              .placeholder = u"Recall from memory",
              .initial_value = {},
              .no_results_message = u""}));
}

TEST_F(
    AutofillPopupControllerImplTest,
    SuggestionFiltering_PresentOnlyWithoutFilterSuggestionsAlwaysFilteredOut) {
  using enum SuggestionType;
  Suggestion suggestion1 = Suggestion(u"abcd", kAddressEntry);
  Suggestion suggestion2 = Suggestion(u"abcd", kAddressEntry);
  suggestion2.filtration_policy =
      Suggestion::FiltrationPolicy::kPresentOnlyWithoutFilter;

  AutofillPopupController& controller =
      client().suggestion_controller(manager());
  ShowSuggestions(manager(), {std::move(suggestion1), std::move(suggestion2)});

  ASSERT_EQ(controller.GetSuggestions().size(), 2u);

  controller.SetFilter(AutofillPopupController::StringFilter(u"ab"),
                       AutofillPopupController::FilterSource::kInputChanged);
  EXPECT_EQ(controller.GetSuggestions().size(), 1u);
}

TEST_F(AutofillPopupControllerImplTest,
       SuggestionFiltering_NonEmptyFilterStatusIsPassedToDelegateOnAccepting) {
  AutofillPopupController& controller =
      client().suggestion_controller(manager());
  test_api(static_cast<AutofillPopupControllerImpl&>(controller))
      .DisableThreshold(true);
  ShowSuggestions(manager(),
                  {Suggestion(u"main_text", SuggestionType::kAddressEntry)});

  EXPECT_CALL(manager().external_delegate(),
              DidAcceptSuggestion(
                  _, EqualsSuggestionMetadata(
                         {.multi_index = {0}, .from_search_result = true})));

  controller.SetFilter(AutofillPopupController::StringFilter(u"main_text"),
                       AutofillPopupController::FilterSource::kInputChanged);
  controller.AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);
}

TEST_F(AutofillPopupControllerImplTest, RemoveSuggestion) {
  ShowSuggestions(manager(),
                  {SuggestionType::kAddressEntry, SuggestionType::kAddressEntry,
                   SuggestionType::kManageAddress});

  // Generate a popup, so it can be hidden later. It doesn't matter what the
  // external_delegate thinks is being shown in the process, since we are just
  // testing the popup here.
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(
      manager().external_delegate(),
      RemoveSuggestion(Field(&Suggestion::type, SuggestionType::kAddressEntry)))
      .WillRepeatedly(Return(true));

  // Remove the first entry. The popup should be redrawn since its size has
  // changed.
  EXPECT_CALL(*client().popup_view(),
              OnSuggestionsChanged(/*prefer_prev_arrow_side=*/false));
  EXPECT_TRUE(client().suggestion_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  Mock::VerifyAndClearExpectations(client().popup_view());

  // Remove the next entry. The popup should then be hidden since there are
  // no Autofill entries left.
  EXPECT_CALL(client().suggestion_controller(manager()),
              Hide(SuggestionHidingReason::kNoSuggestions));
  EXPECT_TRUE(client().suggestion_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
}

// Tests that removing the last manual/actionable Autocomplete suggestion will
// successfully hide the popup, even if the remaining items in the list include
// structural elements (like separators) and a promo button (which is not
// standalone). The promo button and its separator alone should not be enough to
// keep the popup open.
TEST_F(AutofillPopupControllerImplTest,
       RemoveLastAutocompleteSuggestion_HidesPopupEvenWithMemoryPromo) {
  ShowSuggestions(manager(), {SuggestionType::kAutocompleteEntry,
                              SuggestionType::kSeparator,
                              SuggestionType::kAutocompleteAtMemoryButton});

  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(
                  Field(&Suggestion::type, SuggestionType::kAutocompleteEntry)))
      .WillOnce(Return(true));

  EXPECT_CALL(client().suggestion_controller(manager()),
              Hide(SuggestionHidingReason::kNoSuggestions));
  EXPECT_TRUE(client().suggestion_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
}

TEST_F(AutofillPopupControllerImplTest,
       RemoveLastSuggestion_DoesNotHidePopupForAtMemory) {
  ShowSuggestions(manager(), {SuggestionType::kAtMemorySearchResult},
                  AutofillSuggestionTriggerSource::kAtMemoryTriggerString);

  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::type,
                                     SuggestionType::kAtMemorySearchResult)))
      .WillOnce(Return(true));

  EXPECT_CALL(client().suggestion_controller(manager()), Hide)
      .WillRepeatedly(Return());
  EXPECT_CALL(client().suggestion_controller(manager()),
              Hide(SuggestionHidingReason::kNoSuggestions))
      .Times(0);
  EXPECT_CALL(*client().popup_view(),
              OnSuggestionsChanged(/*prefer_prev_arrow_side=*/false));
  EXPECT_TRUE(client().suggestion_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
}

TEST_F(AutofillPopupControllerImplTest,
       RemoveAutocompleteSuggestion_AnnounceText) {
  ShowSuggestions(manager(), {Suggestion(u"main text",
                                         SuggestionType::kAutocompleteEntry)});
  test::GenerateTestAutofillPopup(&manager().external_delegate());

  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(
                  Field(&Suggestion::type, SuggestionType::kAutocompleteEntry)))
      .WillOnce(Return(true));
  EXPECT_CALL(*client().popup_view(),
              AxAnnounce(Eq(u"Entry main text has been deleted")));
  EXPECT_TRUE(client().suggestion_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
}

TEST_F(AutofillPopupControllerImplTest,
       RemoveAutocompleteSuggestion_NoMetricsEmittedOnFail) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {SuggestionType::kAutocompleteEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(
                  Field(&Suggestion::type, SuggestionType::kAutocompleteEntry)))
      .WillOnce(Return(false));

  EXPECT_FALSE(client().suggestion_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      SingleEntryRemovalMethod::kKeyboardShiftDeletePressed, 0);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events3",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 0);
}

TEST_F(AutofillPopupControllerImplTest,
       RemoveAutocompleteSuggestion_MetricsEmittedOnSuccess) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {SuggestionType::kAutocompleteEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(
                  Field(&Suggestion::type, SuggestionType::kAutocompleteEntry)))
      .WillOnce(Return(true));

  EXPECT_TRUE(client().suggestion_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      SingleEntryRemovalMethod::kKeyboardShiftDeletePressed, 1);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events3",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 1);
  // Also no autofill metrics are emitted.
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Popup.Total", 1,
                                      0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.KeyboardAccessory.Total", 1, 0);
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Any.Total", 1,
                                      0);
}

TEST_F(AutofillPopupControllerImplTest,
       RemoveAddressSuggestion_NoMetricsEmittedOnFail) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(
      manager().external_delegate(),
      RemoveSuggestion(Field(&Suggestion::type, SuggestionType::kAddressEntry)))
      .WillOnce(Return(false));

  EXPECT_FALSE(client().suggestion_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Popup.Total", 1,
                                      0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.KeyboardAccessory.Total", 1, 0);
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Any.Total", 1,
                                      0);
}

TEST_F(AutofillPopupControllerImplTest,
       RemoveAddressSuggestion_MetricsEmittedOnSuccess) {
  base::HistogramTester histogram_tester;

  AutofillProfile profile = AutofillProfile(AddressCountryCode("US"));
  personal_data().address_data_manager().AddProfile(profile);
  Suggestion suggestion(SuggestionType::kAddressEntry);
  suggestion.payload =
      Suggestion::AutofillProfilePayload(Suggestion::Guid(profile.guid()));
  ShowSuggestions(manager(), {suggestion});
  test::GenerateTestAutofillPopup(&manager().external_delegate());

  EXPECT_CALL(
      manager().external_delegate(),
      RemoveSuggestion(Field(&Suggestion::type, SuggestionType::kAddressEntry)))
      .WillOnce(Return(true));

  EXPECT_TRUE(client().suggestion_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Any.Total", 1,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.Any.LocalOrSyncable", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.Any.LocalOrSyncable", 1, 1);
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Popup.Total",
                                        1, 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.ProfileDeleted.Popup.LocalOrSyncable", 1, 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.ProfileDeleted.KeyboardAccessory.Total", 1, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.ProfileDeleted.KeyboardAccessory.LocalOrSyncable", 1, 1);
  } else {
    histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Popup.Total",
                                        1, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.ProfileDeleted.Popup.LocalOrSyncable", 1, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.ProfileDeleted.KeyboardAccessory.Total", 1, 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.ProfileDeleted.KeyboardAccessory.LocalOrSyncable", 1, 0);
  }
  // No autocomplete deletion metrics are emitted.
  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      SingleEntryRemovalMethod::kKeyboardShiftDeletePressed, 0);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events3",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 0);
}

TEST_F(AutofillPopupControllerImplTest,
       RemoveCreditCardSuggestion_NoMetricsEmitted) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {SuggestionType::kCreditCardEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(
                  Field(&Suggestion::type, SuggestionType::kCreditCardEntry)))
      .WillOnce(Return(true));

  EXPECT_TRUE(client().suggestion_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      SingleEntryRemovalMethod::kKeyboardShiftDeletePressed, 0);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events3",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 0);
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Popup.Total", 1,
                                      0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.KeyboardAccessory.Total", 1, 0);
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Any.Total", 1,
                                      0);
}

TEST_F(AutofillPopupControllerImplTest, UnselectingClearsPreview) {
  EXPECT_CALL(manager().external_delegate(), ClearPreviewedForm());
  client().suggestion_controller(manager()).UnselectSuggestion();
}

TEST_F(AutofillPopupControllerImplTest,
       HasSuggestionsWebauthnHybridFlowStandalone) {
  ShowSuggestions(manager(),
                  {SuggestionType::kWebauthnSignInWithAnotherDevice});
  AutofillPopupControllerImpl& controller =
      static_cast<AutofillPopupControllerImpl&>(
          client().suggestion_controller(manager()));

  // kWebauthnSignInWithAnotherDevice should be classified as a standalone
  // suggestion type on Desktop, so HasEmptySuggestionContent() evaluates to
  // false!
  EXPECT_FALSE(test_api(controller).HasEmptySuggestionContent());
}

TEST_F(AutofillPopupControllerImplTest,
       HasSuggestionsSeparatorsAndNonStandaloneFootersAreNotStandalone) {
  ShowSuggestions(manager(), {SuggestionType::kSeparator,
                              SuggestionType::kAllSavedPasswordsEntry});
  AutofillPopupControllerImpl& controller =
      static_cast<AutofillPopupControllerImpl&>(
          client().suggestion_controller(manager()));

  // A list containing only a separator or a non-standalone settings footer
  // (like kAllSavedPasswordsEntry) does NOT have any standalone suggestions!
  EXPECT_TRUE(test_api(controller).HasEmptySuggestionContent());
}

#if !BUILDFLAG(IS_CHROMEOS)
class MockAutofillDriver : public ContentAutofillDriver {
 public:
  using ContentAutofillDriver::ContentAutofillDriver;

  MockAutofillDriver(MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(MockAutofillDriver&) = delete;

  ~MockAutofillDriver() override = default;
  MOCK_METHOD(ui::AXTreeID, GetAxTreeId, (), (const override));
};

class AutofillPopupControllerForPopupAxTest
    : public AutofillSuggestionControllerForTest {
 public:
  using AutofillSuggestionControllerForTest::
      AutofillSuggestionControllerForTest;

  using AutofillSuggestionControllerForTest::FireControlsChangedEvent;
  MOCK_METHOD(ui::AXPlatformNode*,
              GetRootAXPlatformNodeForWebContents,
              (),
              (override));
};

class MockAxTreeManager : public ui::AXTreeManager {
 public:
  MockAxTreeManager() = default;
  MockAxTreeManager(MockAxTreeManager&) = delete;
  MockAxTreeManager& operator=(MockAxTreeManager&) = delete;
  ~MockAxTreeManager() override = default;

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
  const ui::AXTreeData& GetTreeData() const override { return tree_data_; }
  ui::AXTreeData& tree_data() { return tree_data_; }

 private:
  ui::AXTreeData tree_data_;
};

class MockAxPlatformNode : public ui::AXPlatformNodeBase {
 public:
  MockAxPlatformNode() = default;
  MockAxPlatformNode(MockAxPlatformNode&) = delete;
  MockAxPlatformNode& operator=(MockAxPlatformNode&) = delete;
  ~MockAxPlatformNode() override = default;

  MOCK_METHOD(bool, IsDestroyed, (), (const override));
  MOCK_METHOD(ui::AXPlatformNodeDelegate*, GetDelegate, (), (const override));
};

using AutofillPopupControllerImplTestAccessibilityBase =
    AutofillSuggestionControllerTestBase<
        TestAutofillPopupControllerAutofillClient<
            NiceMock<AutofillPopupControllerForPopupAxTest>>,
        NiceMock<MockAutofillDriver>>;
class AutofillPopupControllerImplTestAccessibility
    : public AutofillPopupControllerImplTestAccessibilityBase {
 public:
  static constexpr int kAxUniqueId = 123;

  AutofillPopupControllerImplTestAccessibility()
      : accessibility_mode_override_(ui::AXMode::kExtendedProperties) {}
  AutofillPopupControllerImplTestAccessibility(
      AutofillPopupControllerImplTestAccessibility&) = delete;
  AutofillPopupControllerImplTestAccessibility& operator=(
      AutofillPopupControllerImplTestAccessibility&) = delete;
  ~AutofillPopupControllerImplTestAccessibility() override = default;

  void SetUp() override {
    AutofillPopupControllerImplTestAccessibilityBase::SetUp();

    ON_CALL(driver(), GetAxTreeId()).WillByDefault(Return(test_tree_id_));
    ON_CALL(client().suggestion_controller(manager()),
            GetRootAXPlatformNodeForWebContents)
        .WillByDefault(Return(&mock_ax_platform_node_));
    ON_CALL(mock_ax_platform_node_, IsDestroyed).WillByDefault(Return(false));
    ON_CALL(mock_ax_platform_node_, GetDelegate)
        .WillByDefault(Return(&mock_ax_platform_node_delegate_));
    mock_ax_platform_node_delegate_.tree_data().focused_tree_id = test_tree_id_;
    ON_CALL(*client().popup_view(), GetAxUniqueId)
        .WillByDefault(Return(std::optional<int32_t>(kAxUniqueId)));
    ON_CALL(mock_ax_platform_node_delegate_, GetFromTreeIDAndNodeID)
        .WillByDefault(Return(&mock_ax_platform_node_));
  }

  void TearDown() override {
    // This needs to bo reset explicit because having the mode set to
    // `kExtendedProperties` causes mocked functions to get called  with
    // `mock_ax_platform_node_delegate` after it has been destroyed.
    accessibility_mode_override_.ResetMode();
    AutofillPopupControllerImplTestAccessibilityBase::TearDown();
  }

 protected:
  content::ScopedAccessibilityModeOverride accessibility_mode_override_;
  NiceMock<MockAxPlatformNodeDelegate> mock_ax_platform_node_delegate_;
  NiceMock<MockAxPlatformNode> mock_ax_platform_node_;
  ui::AXTreeID test_tree_id_ = ui::AXTreeID::CreateNewAXTreeID();
};

// Test for successfully firing controls changed event for popup show/hide.
TEST_F(AutofillPopupControllerImplTestAccessibility,
       FireControlsChangedEventDuringShowAndHide) {
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  // Manually fire the event for popup show since setting the test view results
  // in the fire controls changed event not being sent.
  client().suggestion_controller(manager()).FireControlsChangedEvent(true);
  EXPECT_EQ(kAxUniqueId, ui::GetActivePopupAxUniqueId());

  client().suggestion_controller(manager()).DoHide();
  EXPECT_EQ(std::nullopt, ui::GetActivePopupAxUniqueId());
}

// Test for attempting to fire controls changed event when ax tree manager
// fails to retrieve the ax platform node associated with the popup.
// No event is fired and global active popup ax unique id is not set.
TEST_F(AutofillPopupControllerImplTestAccessibility,
       FireControlsChangedEventNoAxPlatformNode) {
  EXPECT_CALL(mock_ax_platform_node_delegate_, GetFromTreeIDAndNodeID)
      .WillOnce(Return(nullptr));

  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  // Manually fire the event for popup show since setting the test view results
  // in the fire controls changed event not being sent.
  client().suggestion_controller(manager()).FireControlsChangedEvent(true);
  EXPECT_EQ(std::nullopt, ui::GetActivePopupAxUniqueId());
}

// Test for attempting to fire controls changed event when failing to retrieve
// the autofill popup's ax unique id. No event is fired and the global active
// popup ax unique id is not set.
TEST_F(AutofillPopupControllerImplTestAccessibility,
       FireControlsChangedEventNoPopupAxUniqueId) {
  EXPECT_CALL(*client().popup_view(), GetAxUniqueId)
      .WillOnce(Return(std::nullopt));

  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  // Manually fire the event for popup show since setting the test view results
  // in the fire controls changed event not being sent.
  client().suggestion_controller(manager()).FireControlsChangedEvent(true);
  EXPECT_EQ(std::nullopt, ui::GetActivePopupAxUniqueId());
}

// Test for attempting to fire controls changed event on hide when ax tree
// manager fails to retrieve the ax platform node associated with the popup.
// The global active popup ax unique id should still be cleared.
TEST_F(AutofillPopupControllerImplTestAccessibility,
       FireControlsChangedEventHideClearsActivePopupAxUniqueId) {
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  client().suggestion_controller(manager()).FireControlsChangedEvent(true);
  EXPECT_EQ(ui::GetActivePopupAxUniqueId(), kAxUniqueId);

  // Simulate failure to retrieve the target node on hide.
  EXPECT_CALL(mock_ax_platform_node_delegate_, GetFromTreeIDAndNodeID)
      .WillOnce(Return(nullptr));

  client().suggestion_controller(manager()).DoHide();
  EXPECT_EQ(ui::GetActivePopupAxUniqueId(), std::nullopt);
}

// Test for attempting to fire controls changed event when focused tree ID is
// unknown.
TEST_F(AutofillPopupControllerImplTestAccessibility,
       FireControlsChangedEventUnknownTreeId) {
  mock_ax_platform_node_delegate_.tree_data().focused_tree_id =
      ui::AXTreeIDUnknown();
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});
  client().suggestion_controller(manager()).FireControlsChangedEvent(true);
  EXPECT_EQ(ui::GetActivePopupAxUniqueId(), std::nullopt);
}
#endif

}  // namespace
}  // namespace autofill
