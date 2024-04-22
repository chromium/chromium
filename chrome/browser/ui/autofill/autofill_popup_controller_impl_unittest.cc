// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"

#include <optional>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller_test_base.h"
#include "chrome/browser/ui/autofill/test_autofill_popup_controller_autofill_client.h"
#include "components/autofill/core/browser/ui/suggestion.h"
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

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "content/public/test/scoped_accessibility_mode_override.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

namespace autofill {

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;

using SingleEntryRemovalMethod =
    autofill::AutofillMetrics::SingleEntryRemovalMethod;

Matcher<const AutofillPopupDelegate::SuggestionPosition&>
EqualsSuggestionPosition(AutofillPopupDelegate::SuggestionPosition position) {
  return AllOf(
      Field(&AutofillPopupDelegate::SuggestionPosition::row, position.row),
      Field(&AutofillPopupDelegate::SuggestionPosition::sub_popup_level,
            position.sub_popup_level));
}

}  // namespace

using AutofillPopupControllerImplTest = AutofillSuggestionControllerTestBase<
    TestAutofillPopupControllerAutofillClient<>>;

TEST_F(AutofillPopupControllerImplTest, SubPopupIsCreatedWithViewFromParent) {
  base::WeakPtr<AutofillSuggestionController> sub_controller =
      client().popup_controller(manager()).OpenSubPopup(
          {0, 0, 10, 10}, {}, AutoselectFirstSuggestion(false));
  EXPECT_TRUE(sub_controller);
}

TEST_F(AutofillPopupControllerImplTest,
       DelegateMethodsAreCalledOnlyByRootPopup) {
  EXPECT_CALL(manager().external_delegate(), OnPopupShown()).Times(0);
  base::WeakPtr<AutofillSuggestionController> sub_controller =
      client().popup_controller(manager()).OpenSubPopup(
          {0, 0, 10, 10}, {}, AutoselectFirstSuggestion(false));

  EXPECT_CALL(manager().external_delegate(), OnPopupHidden()).Times(0);
  sub_controller->Hide(PopupHidingReason::kUserAborted);

  EXPECT_CALL(manager().external_delegate(), OnPopupHidden());
  client().popup_controller(manager()).Hide(PopupHidingReason::kUserAborted);
}

TEST_F(AutofillPopupControllerImplTest, EventsAreDelegatedToChildrenAndView) {
  EXPECT_CALL(manager().external_delegate(), OnPopupShown()).Times(0);
  base::WeakPtr<AutofillSuggestionController> sub_controller =
      client().popup_controller(manager()).OpenSubPopup(
          {0, 0, 10, 10}, {}, AutoselectFirstSuggestion(false));

  content::NativeWebKeyboardEvent event = CreateKeyPressEvent(ui::VKEY_LEFT);
  EXPECT_CALL(*client().sub_popup_view(), HandleKeyPressEvent)
      .WillOnce(Return(true));
  EXPECT_CALL(*client().popup_view(), HandleKeyPressEvent).Times(0);
  EXPECT_TRUE(client().popup_controller(manager()).HandleKeyPressEvent(event));

  EXPECT_CALL(*client().sub_popup_view(), HandleKeyPressEvent)
      .WillOnce(Return(false));
  EXPECT_CALL(*client().popup_view(), HandleKeyPressEvent).Times(1);
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

// The second popup is also the second "sub_popup_level". This test asserts that
// the information regarding the popup level is passed on to the delegate.
TEST_F(AutofillPopupControllerImplTest, PopupForwardsSuggestionPosition) {
  base::WeakPtr<AutofillSuggestionController> sub_controller =
      client().popup_controller(manager()).OpenSubPopup(
          {0, 0, 10, 10}, {Suggestion(PopupItemId::kAddressEntry)},
          AutoselectFirstSuggestion(false));
  ASSERT_TRUE(sub_controller);
  static_cast<AutofillPopupControllerImpl*>(sub_controller.get())
      ->SetViewForTesting(client().sub_popup_view()->GetWeakPtr());

  EXPECT_CALL(manager().external_delegate(),
              DidAcceptSuggestion(_, EqualsSuggestionPosition(
                                         {.row = 0, .sub_popup_level = 1})));

  task_environment()->FastForwardBy(base::Milliseconds(1000));
  sub_controller->AcceptSuggestion(/*index=*/0);
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

// Tests that the popup controller queries the view for its screen location.
TEST_F(AutofillPopupControllerImplTest, GetPopupScreenLocationCallsView) {
  ShowSuggestions(manager(), {PopupItemId::kCompose});

  using PopupScreenLocation = AutofillClient::PopupScreenLocation;
  constexpr gfx::Rect kSampleRect = gfx::Rect(123, 234);
  EXPECT_CALL(*client().popup_view(), GetPopupScreenLocation)
      .WillOnce(Return(PopupScreenLocation{.bounds = kSampleRect}));
  EXPECT_THAT(client().popup_controller(manager()).GetPopupScreenLocation(),
              Optional(Field(&PopupScreenLocation::bounds, kSampleRect)));
}

// Tests that a change to a text field hides a popup with a Compose suggestion.
TEST_F(AutofillPopupControllerImplTest, HidesOnFieldChangeForComposeEntries) {
  ShowSuggestions(manager(), {PopupItemId::kCompose});
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kFieldValueChanged));
  manager().NotifyObservers(
      &AutofillManager::Observer::OnBeforeTextFieldDidChange, FormGlobalId(),
      FieldGlobalId());
}

// Tests that Compose saved state notification popup gets hidden after 2
// seconds, but not after 1 second.
TEST_F(AutofillPopupControllerImplTest,
       TimedHideComposeSavedStateNotification) {
  ShowSuggestions(manager(), {PopupItemId::kComposeSavedStateNotification});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  ::testing::MockFunction<void()> check;
  {
    ::testing::InSequence s;
    EXPECT_CALL(check, Call);
    EXPECT_CALL(client().popup_controller(manager()),
                Hide(PopupHidingReason::kFadeTimerExpired));
  }
  task_environment()->FastForwardBy(base::Seconds(1));
  check.Call();
  task_environment()->FastForwardBy(base::Seconds(1));
  Mock::VerifyAndClearExpectations(&client().popup_controller(manager()));
}

TEST_F(AutofillPopupControllerImplTest,
       PopupHidesOnWebContentsFocusLossIfViewIsNotFocused) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});

  EXPECT_CALL(*client().popup_view(), HasFocus).WillOnce(Return(false));
  EXPECT_CALL(*client().popup_view(), Hide);
  client().popup_controller(manager()).Hide(PopupHidingReason::kFocusChanged);

  Mock::VerifyAndClearExpectations(client().popup_view());
}

TEST_F(AutofillPopupControllerImplTest,
       PopupDoesntHideOnWebContentsFocusLossIfViewIsFocused) {
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});

  EXPECT_CALL(*client().popup_view(), HasFocus).WillOnce(Return(true));
  EXPECT_CALL(*client().popup_view(), Hide).Times(0);
  client().popup_controller(manager()).Hide(PopupHidingReason::kFocusChanged);

  Mock::VerifyAndClearExpectations(client().popup_view());
}

TEST_F(AutofillPopupControllerImplTest,
       RemoveAutocompleteSuggestion_IgnoresClickOutsideCheck) {
  ShowSuggestions(manager(), {PopupItemId::kAutocompleteEntry,
                              PopupItemId::kAutocompleteEntry});

  // Generate a popup, so it can be hidden later. It doesn't matter what the
  // external_delegate thinks is being shown in the process, since we are just
  // testing the popup here.
  test::GenerateTestAutofillPopup(&manager().external_delegate());

  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kAutocompleteEntry)))
      .WillOnce(Return(true));
  // Remove the first entry. The popup should be redrawn since its size has
  // changed.
  EXPECT_CALL(*client().popup_view(), OnSuggestionsChanged());
  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      0,
      AutofillMetrics::SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  Mock::VerifyAndClearExpectations(client().popup_view());

  EXPECT_TRUE(client()
                  .popup_controller(manager())
                  .ShouldIgnoreMouseObservedOutsideItemBoundsCheck());
}

// Tests that if the popup is shown in the *main frame*, changing the zoom hides
// the popup.
TEST_F(AutofillPopupControllerImplTest, HideInMainFrameOnZoomChange) {
  zoom::ZoomController::CreateForWebContents(web_contents());
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  // Triggered by OnZoomChanged().
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kContentAreaMoved));
  // Override the default ON_CALL behavior to do nothing to avoid destroying the
  // hide helper. We want to test ZoomObserver events explicitly.
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kWidgetChanged))
      .WillOnce(Return());
  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents());
  zoom_controller->SetZoomLevel(zoom_controller->GetZoomLevel() + 1.0);
  // Verify and clear before TearDown() closes the popup.
  Mock::VerifyAndClearExpectations(&client().popup_controller(manager()));
}

TEST_F(AutofillPopupControllerImplTest,
       SuggestionFiltration_NoFilteringByDefault) {
  AutofillPopupController& controller = client().popup_controller(manager());
  ShowSuggestions(manager(), {Suggestion(u"abc")});

  EXPECT_EQ(controller.GetSuggestions().size(), 1u);
  EXPECT_EQ(controller.GetSuggestionFilterMatches().size(), 0u);
}

TEST_F(AutofillPopupControllerImplTest,
       SuggestionFiltration_SuggestionChangeNotifications) {
  AutofillPopupController& controller = client().popup_controller(manager());
  ShowSuggestions(manager(), {
                                 Suggestion(u"abc"),
                                 Suggestion(u"axx"),
                             });

  EXPECT_CALL(*client().popup_view(), OnSuggestionsChanged());
  controller.SetFilter(AutofillPopupController::SuggestionFilter(u"ab"));

  EXPECT_CALL(*client().popup_view(), OnSuggestionsChanged());
  controller.SetFilter(std::nullopt);
}

TEST_F(AutofillPopupControllerImplTest, SuggestionFiltration_MatchingMainText) {
  AutofillPopupController& controller = client().popup_controller(manager());
  ShowSuggestions(manager(), {
                                 Suggestion(u"abc"),
                                 Suggestion(u"abx"),
                                 Suggestion(u"axx"),
                             });

  EXPECT_EQ(controller.GetSuggestions().size(), 3u);
  EXPECT_EQ(controller.GetSuggestionFilterMatches().size(), 0u);

  controller.SetFilter(AutofillPopupController::SuggestionFilter(u"ab"));
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
       SuggestionFiltration_SuggestionIsDeletedFromFilteredList) {
  AutofillPopupController& controller = client().popup_controller(manager());
  ShowSuggestions(manager(), {
                                 Suggestion(u"abc"),
                                 Suggestion(u"abx"),
                                 Suggestion(u"axx"),
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

TEST_F(AutofillPopupControllerImplTest, RemoveSuggestion) {
  ShowSuggestions(manager(),
                  {PopupItemId::kAddressEntry, PopupItemId::kAddressEntry,
                   PopupItemId::kAutofillOptions});

  // Generate a popup, so it can be hidden later. It doesn't matter what the
  // external_delegate thinks is being shown in the process, since we are just
  // testing the popup here.
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kAddressEntry)))
      .WillRepeatedly(Return(true));

  // Remove the first entry. The popup should be redrawn since its size has
  // changed.
  EXPECT_CALL(*client().popup_view(), OnSuggestionsChanged());
  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  Mock::VerifyAndClearExpectations(client().popup_view());

  // Remove the next entry. The popup should then be hidden since there are
  // no Autofill entries left.
  EXPECT_CALL(client().popup_controller(manager()),
              Hide(PopupHidingReason::kNoSuggestions));
  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
}

TEST_F(AutofillPopupControllerImplTest,
       RemoveAutocompleteSuggestion_AnnounceText) {
  ShowSuggestions(manager(),
                  {Suggestion(u"main text", PopupItemId::kAutocompleteEntry)});
  test::GenerateTestAutofillPopup(&manager().external_delegate());

  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kAutocompleteEntry)))
      .WillOnce(Return(true));
  EXPECT_CALL(*client().popup_view(),
              AxAnnounce(Eq(u"Entry main text has been deleted")));
  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
}

TEST_F(AutofillPopupControllerImplTest,
       RemoveAutocompleteSuggestion_NoMetricsEmittedOnFail) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {PopupItemId::kAutocompleteEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kAutocompleteEntry)))
      .WillOnce(Return(false));

  EXPECT_FALSE(client().popup_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      SingleEntryRemovalMethod::kKeyboardShiftDeletePressed, 0);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events2",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 0);
}

TEST_F(AutofillPopupControllerImplTest,
       RemoveAutocompleteSuggestion_MetricsEmittedOnSuccess) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {PopupItemId::kAutocompleteEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kAutocompleteEntry)))
      .WillOnce(Return(true));

  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      SingleEntryRemovalMethod::kKeyboardShiftDeletePressed, 1);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events2",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 1);
  // Also no autofill metrics are emitted.
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Popup", 1, 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.KeyboardAccessory", 1, 0);
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Any", 1, 0);
}

TEST_F(AutofillPopupControllerImplTest,
       RemoveAddressSuggestion_NoMetricsEmittedOnFail) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kAddressEntry)))
      .WillOnce(Return(false));

  EXPECT_FALSE(client().popup_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Popup", 1, 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.KeyboardAccessory", 1, 0);
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Any", 1, 0);
}

TEST_F(AutofillPopupControllerImplTest,
       RemoveAddressSuggestion_MetricsEmittedOnSuccess) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kAddressEntry)))
      .WillOnce(Return(true));

  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Any", 1, 1);
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Popup", 1, 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.ProfileDeleted.KeyboardAccessory", 1, 1);
  } else {
    histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Popup", 1, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.ProfileDeleted.KeyboardAccessory", 1, 0);
  }
  // No autocomplete deletion metrics are emitted.
  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      SingleEntryRemovalMethod::kKeyboardShiftDeletePressed, 0);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events2",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 0);
}

TEST_F(AutofillPopupControllerImplTest,
       RemoveCreditCardSuggestion_NoMetricsEmitted) {
  base::HistogramTester histogram_tester;
  ShowSuggestions(manager(), {PopupItemId::kCreditCardEntry});
  test::GenerateTestAutofillPopup(&manager().external_delegate());
  EXPECT_CALL(manager().external_delegate(),
              RemoveSuggestion(Field(&Suggestion::popup_item_id,
                                     PopupItemId::kCreditCardEntry)))
      .WillOnce(Return(true));

  EXPECT_TRUE(client().popup_controller(manager()).RemoveSuggestion(
      0, SingleEntryRemovalMethod::kKeyboardShiftDeletePressed));
  histogram_tester.ExpectUniqueSample(
      "Autofill.Autocomplete.SingleEntryRemovalMethod",
      SingleEntryRemovalMethod::kKeyboardShiftDeletePressed, 0);
  histogram_tester.ExpectUniqueSample(
      "Autocomplete.Events2",
      AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_DELETED, 0);
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Popup", 1, 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ProfileDeleted.KeyboardAccessory", 1, 0);
  histogram_tester.ExpectUniqueSample("Autofill.ProfileDeleted.Any", 1, 0);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
namespace {

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

}  // namespace

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
      : accessibility_mode_override_(ui::AXMode::kScreenReader) {}
  AutofillPopupControllerImplTestAccessibility(
      AutofillPopupControllerImplTestAccessibility&) = delete;
  AutofillPopupControllerImplTestAccessibility& operator=(
      AutofillPopupControllerImplTestAccessibility&) = delete;
  ~AutofillPopupControllerImplTestAccessibility() override = default;

  void SetUp() override {
    AutofillPopupControllerImplTestAccessibilityBase::SetUp();

    ON_CALL(driver(), GetAxTreeId()).WillByDefault(Return(test_tree_id_));
    ON_CALL(client().popup_controller(manager()),
            GetRootAXPlatformNodeForWebContents)
        .WillByDefault(Return(&mock_ax_platform_node_));
    ON_CALL(mock_ax_platform_node_, GetDelegate)
        .WillByDefault(Return(&mock_ax_platform_node_delegate_));
    ON_CALL(*client().popup_view(), GetAxUniqueId)
        .WillByDefault(Return(std::optional<int32_t>(kAxUniqueId)));
    ON_CALL(mock_ax_platform_node_delegate_, GetFromTreeIDAndNodeID)
        .WillByDefault(Return(&mock_ax_platform_node_));
  }

  void TearDown() override {
    // This needs to bo reset explicit because having the mode set to
    // `kScreenReader` causes mocked functions to get called  with
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
  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  // Manually fire the event for popup show since setting the test view results
  // in the fire controls changed event not being sent.
  client().popup_controller(manager()).FireControlsChangedEvent(true);
  EXPECT_EQ(kAxUniqueId, ui::GetActivePopupAxUniqueId());

  client().popup_controller(manager()).DoHide();
  EXPECT_EQ(std::nullopt, ui::GetActivePopupAxUniqueId());
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
  EXPECT_EQ(std::nullopt, ui::GetActivePopupAxUniqueId());
}

// Test for attempting to fire controls changed event when failing to retrieve
// the autofill popup's ax unique id. No event is fired and the global active
// popup ax unique id is not set.
TEST_F(AutofillPopupControllerImplTestAccessibility,
       FireControlsChangedEventNoPopupAxUniqueId) {
  EXPECT_CALL(*client().popup_view(), GetAxUniqueId)
      .WillOnce(Return(std::nullopt));

  ShowSuggestions(manager(), {PopupItemId::kAddressEntry});
  // Manually fire the event for popup show since setting the test view results
  // in the fire controls changed event not being sent.
  client().popup_controller(manager()).FireControlsChangedEvent(true);
  EXPECT_EQ(std::nullopt, ui::GetActivePopupAxUniqueId());
}
#endif

}  // namespace autofill
