// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"

#include <optional>

#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl_test_api.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_test_base.h"
#include "chrome/browser/ui/autofill/test_autofill_popup_controller_autofill_client.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/ui/popup_interaction.h"
#include "components/autofill/core/browser/ui/suggestion_button_action.h"
#include "components/autofill/core/common/aliases.h"
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
using ::testing::NiceMock;
using ::testing::Return;

using SingleEntryRemovalMethod =
    autofill::AutofillMetrics::SingleEntryRemovalMethod;

Matcher<const AutofillSuggestionDelegate::SuggestionMetadata&>
EqualsSuggestionMetadata(
    AutofillSuggestionDelegate::SuggestionMetadata metadata) {
  return AllOf(
      Field(&AutofillSuggestionDelegate::SuggestionMetadata::row, metadata.row),
      Field(&AutofillSuggestionDelegate::SuggestionMetadata::sub_popup_level,
            metadata.sub_popup_level),
      Field(&AutofillSuggestionDelegate::SuggestionMetadata::from_search_result,
            metadata.from_search_result));
}

using AutofillPopupControllerImplTest = AutofillSuggestionControllerTestBase<
    TestAutofillPopupControllerAutofillClient<>>;

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
          {0, 0, 10, 10}, {Suggestion(SuggestionType::kAddressEntry)},
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
       DelegateMethodsAreCalledOnlyByRootPopup) {
  EXPECT_CALL(manager().external_delegate(), OnSuggestionsShown).Times(0);
  ON_CALL(*client().sub_popup_view(), Show).WillByDefault(Return(true));
  base::WeakPtr<AutofillSuggestionController> sub_controller =
      client().suggestion_controller(manager()).OpenSubPopup(
          {0, 0, 10, 10}, {}, AutoselectFirstSuggestion(false));

  EXPECT_CALL(manager().external_delegate(), OnSuggestionsHidden()).Times(0);
  sub_controller->Hide(SuggestionHidingReason::kUserAborted);

  EXPECT_CALL(manager().external_delegate(), OnSuggestionsHidden());
  client().suggestion_controller(manager()).Hide(
      SuggestionHidingReason::kUserAborted);
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

// Tests that the controller forwards calls to perform a button action (such as
// clicking a close button on a suggestion) to its delegate.
TEST_F(AutofillPopupControllerImplTest, ButtonActionsAreSentToDelegate) {
  ShowSuggestions(manager(), {SuggestionType::kComposeResumeNudge});
  EXPECT_CALL(manager().external_delegate(),
              DidPerformButtonActionForSuggestion);
  client().suggestion_controller(manager()).PerformButtonActionForSuggestion(
      0, SuggestionButtonAction());
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
              DidAcceptSuggestion(_, EqualsSuggestionMetadata(
                                         {.row = 0, .sub_popup_level = 1})));

  task_environment()->FastForwardBy(base::Milliseconds(1000));
  sub_controller->AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);
}

TEST_F(AutofillPopupControllerImplTest, DoesNotAcceptUnacceptableSuggestions) {
  Suggestion suggestion(u"Open the pod bay doors, HAL",
                        SuggestionType::kAutocompleteEntry);
  suggestion.acceptability = Suggestion::Acceptability::kUnacceptable;
  ShowSuggestions(manager(), {std::move(suggestion)});

  EXPECT_CALL(manager().external_delegate(), DidAcceptSuggestion).Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(1000));
  client().suggestion_controller(manager()).AcceptSuggestion(
      /*index=*/0, AutofillMetrics::SuggestionAcceptedMethod::kMouse);
}

TEST_F(AutofillPopupControllerImplTest, DoesNotSelectUnacceptableSuggestions) {
  Suggestion suggestion(u"I'm sorry, Dave. I'm afraid I can't do that.",
                        SuggestionType::kAutocompleteEntry);
  suggestion.acceptability = Suggestion::Acceptability::kUnacceptable;
  ShowSuggestions(manager(), {std::move(suggestion)});

  EXPECT_CALL(manager().external_delegate(), DidSelectSuggestion).Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(1000));
  client().suggestion_controller(manager()).SelectSuggestion(/*index=*/0);
}

TEST_F(AutofillPopupControllerImplTest,
       ManualFallBackTriggerSource_IgnoresClickOutsideCheck) {
  ShowSuggestions(
      manager(), {SuggestionType::kAddressEntry},
      AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess);

  // Generate a popup, so it can be hidden later. It doesn't matter what the
  // external_delegate thinks is being shown in the process, since we are just
  // testing the popup here.
  test::GenerateTestAutofillPopup(&manager().external_delegate());

  EXPECT_TRUE(client()
                  .suggestion_controller(manager())
                  .ShouldIgnoreMouseObservedOutsideItemBoundsCheck());
}

TEST_F(AutofillPopupControllerImplTest,
       PlusAddressUpdateTriggerSource_IgnoresClickOutsideCheck) {
  ShowSuggestions(
      manager(), {SuggestionType::kAddressEntry},
      AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess);
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_TRUE(client()
                  .suggestion_controller(manager())
                  .ShouldIgnoreMouseObservedOutsideItemBoundsCheck());
}

// Tests that the popup controller queries the view for its screen location.
TEST_F(AutofillPopupControllerImplTest, GetPopupScreenLocationCallsView) {
  ShowSuggestions(manager(), {SuggestionType::kComposeResumeNudge});

  using PopupScreenLocation = AutofillClient::PopupScreenLocation;
  constexpr gfx::Rect kSampleRect = gfx::Rect(123, 234);
  EXPECT_CALL(*client().popup_view(), GetPopupScreenLocation)
      .WillOnce(Return(PopupScreenLocation{.bounds = kSampleRect}));
  EXPECT_THAT(
      client().suggestion_controller(manager()).GetPopupScreenLocation(),
      Optional(Field(&PopupScreenLocation::bounds, kSampleRect)));
}

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

TEST_F(AutofillPopupControllerImplTest,
       PopupDoesntHideOnEndEditingFromRendererIfViewIsFocused) {
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});

  EXPECT_CALL(*client().popup_view(), HasFocus).WillOnce(Return(true));
  EXPECT_CALL(*client().popup_view(), Hide).Times(0);
  client().suggestion_controller(manager()).Hide(
      SuggestionHidingReason::kEndEditing);

  Mock::VerifyAndClearExpectations(client().popup_view());
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
  // Override the default ON_CALL behavior to do nothing to avoid destroying the
  // hide helper. We want to test ZoomObserver events explicitly.
  EXPECT_CALL(client().suggestion_controller(manager()),
              Hide(SuggestionHidingReason::kWidgetChanged))
      .WillOnce(Return());
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
  controller.SetFilter(AutofillPopupController::SuggestionFilter(u"ab"));

  EXPECT_CALL(*client().popup_view(),
              OnSuggestionsChanged(/*prefer_prev_arrow_side=*/true));
  controller.SetFilter(std::nullopt);
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

  controller.SetFilter(AutofillPopupController::SuggestionFilter(u"Ab"));
  EXPECT_EQ(controller.GetSuggestions().size(), 2u);
  EXPECT_EQ(controller.GetSuggestionFilterMatches().size(), 2u);
  EXPECT_THAT(controller.GetSuggestionFilterMatches(),
              ::testing::ElementsAre(
                  AutofillPopupController::SuggestionFilterMatch{
                      .main_text_match = gfx::Range(0, 2),
                  },
                  AutofillPopupController::SuggestionFilterMatch{
                      .main_text_match = gfx::Range(0, 2),
                  }));

  controller.SetFilter(AutofillPopupController::SuggestionFilter(u"abcdefg"));
  EXPECT_EQ(controller.GetSuggestions().size(), 0u);
  EXPECT_EQ(controller.GetSuggestionFilterMatches().size(), 0u);

  controller.SetFilter(std::nullopt);
  EXPECT_EQ(controller.GetSuggestions().size(), 3u);
  EXPECT_EQ(controller.GetSuggestionFilterMatches().size(), 0u);
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

  controller.SetFilter(AutofillPopupController::SuggestionFilter(u"ab"));
  EXPECT_EQ(controller.GetSuggestions().size(), 2u);

  EXPECT_CALL(manager().external_delegate(), RemoveSuggestion)
      .WillOnce(Return(true));
  controller.RemoveSuggestion(
      0, AutofillMetrics::SingleEntryRemovalMethod::kDeleteButtonClicked);
  EXPECT_EQ(controller.GetSuggestions().size(), 1u);
  EXPECT_EQ(controller.GetSuggestionFilterMatches().size(), 1u);

  controller.SetFilter(std::nullopt);
  EXPECT_EQ(controller.GetSuggestions().size(), 2u);
  EXPECT_EQ(controller.GetSuggestionFilterMatches().size(), 0u);
}

TEST_F(AutofillPopupControllerImplTest,
       SuggestionFiltering_StaticSuggestionsAreNotFilteredOut) {
  using enum SuggestionType;

  Suggestion footer_suggestion1 = Suggestion(kSeparator);
  footer_suggestion1.filtration_policy = Suggestion::FiltrationPolicy::kStatic;
  Suggestion footer_suggestion2 = Suggestion(kUndoOrClear);
  footer_suggestion2.filtration_policy = Suggestion::FiltrationPolicy::kStatic;

  AutofillPopupController& controller =
      client().suggestion_controller(manager());
  ShowSuggestions(manager(), {
                                 Suggestion(u"abc", kAddressEntry),
                                 Suggestion(u"abx", kAddressEntry),
                                 std::move(footer_suggestion1),
                                 std::move(footer_suggestion2),
                             });

  controller.SetFilter(AutofillPopupController::SuggestionFilter(u"ab"));
  EXPECT_EQ(controller.GetSuggestions().size(), 4u);
  EXPECT_THAT(controller.GetSuggestions(),
              ElementsAre(Field(&Suggestion::type, kAddressEntry),
                          Field(&Suggestion::type, kAddressEntry),
                          Field(&Suggestion::type, kSeparator),
                          Field(&Suggestion::type, kUndoOrClear)));

  controller.SetFilter(AutofillPopupController::SuggestionFilter(u"abc"));
  EXPECT_EQ(controller.GetSuggestions().size(), 3u);
  EXPECT_THAT(controller.GetSuggestions(),
              ElementsAre(Field(&Suggestion::type, kAddressEntry),
                          Field(&Suggestion::type, kSeparator),
                          Field(&Suggestion::type, kUndoOrClear)));

  controller.SetFilter(AutofillPopupController::SuggestionFilter(u"abcdef"));
  EXPECT_EQ(controller.GetSuggestions().size(), 2u);
  EXPECT_THAT(controller.GetSuggestions(),
              ElementsAre(Field(&Suggestion::type, kSeparator),
                          Field(&Suggestion::type, kUndoOrClear)));
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

  controller.SetFilter(AutofillPopupController::SuggestionFilter(u"ab"));
  EXPECT_FALSE(controller.HasFilteredOutSuggestions());

  controller.SetFilter(AutofillPopupController::SuggestionFilter(u"abc"));
  EXPECT_TRUE(controller.HasFilteredOutSuggestions());
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

  controller.SetFilter(AutofillPopupController::SuggestionFilter(u"ab"));
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
                  _, EqualsSuggestionMetadata({.from_search_result = true})));

  controller.SetFilter(AutofillPopupController::SuggestionFilter(u"main_text"));
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
  MockAxPlatformNodeDelegate mock_ax_platform_node_delegate_;
  MockAxPlatformNode mock_ax_platform_node_;
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
#endif

}  // namespace
}  // namespace autofill
