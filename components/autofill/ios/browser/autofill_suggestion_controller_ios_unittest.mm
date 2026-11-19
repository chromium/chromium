// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_suggestion_controller_ios.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/core/browser/ui/mock_autofill_suggestion_delegate.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;

class AutofillSuggestionControllerIOSTest : public PlatformTest {
 protected:
  AutofillSuggestionControllerIOSTest() {
    ON_CALL(mock_delegate_, GetMainFillingProduct)
        .WillByDefault(Return(FillingProduct::kAddress));
  }

  std::vector<Suggestion> CreateTestSuggestions() {
    std::vector<Suggestion> suggestions;
    suggestions.emplace_back(u"John Doe", SuggestionType::kAddressEntry);
    suggestions.emplace_back(u"Jane Doe", SuggestionType::kAddressEntry);
    return suggestions;
  }

  // Returns the id of the field with `renderer_id` in the main fake frame.
  FieldGlobalId MainFrameFieldId(FieldRendererId renderer_id) {
    return FieldGlobalId(
        LocalFrameToken(*DeserializeJavaScriptFrameId(web::kMainFakeFrameId)),
        renderer_id);
  }

  FieldGlobalId test_field_id() { return MainFrameFieldId(FieldRendererId(1)); }
  FormGlobalId test_form_id() { return FormGlobalId(); }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  NiceMock<MockAutofillSuggestionDelegate> mock_delegate_;
  AutofillSuggestionControllerIOS controller_;
};

// Tests unique session ID generation and association during Show/Hide.
TEST_F(AutofillSuggestionControllerIOSTest, SessionIdGenerationAndPersistence) {
  EXPECT_FALSE(controller_.is_showing());
  EXPECT_EQ(controller_.GetUiSessionId(), std::nullopt);

  auto session_id = GenerateSuggestionUiSessionId();
  EXPECT_EQ(
      controller_.Show(
          CreateTestSuggestions(),
          AutofillSuggestionTriggerSource::kFormControlElementClicked,
          mock_delegate_.GetWeakPtr(), test_form_id(), test_field_id(),
          session_id),
      session_id);

  EXPECT_TRUE(controller_.is_showing());
  EXPECT_EQ(controller_.GetUiSessionId(), session_id);

  controller_.Hide(SuggestionHidingReason::kUserAborted);

  EXPECT_FALSE(controller_.is_showing());
  EXPECT_EQ(controller_.GetUiSessionId(), std::nullopt);
}

// Tests that `Show()` notifies the delegate and sets suggestion data.
TEST_F(AutofillSuggestionControllerIOSTest, ShowNotifiesDelegateAndSetsState) {
  EXPECT_CALL(mock_delegate_, OnSuggestionsShown(_, _));
  EXPECT_CALL(mock_delegate_, GetMainFillingProduct())
      .WillRepeatedly(Return(FillingProduct::kAddress));

  controller_.Show(CreateTestSuggestions(),
                   AutofillSuggestionTriggerSource::kFormControlElementClicked,
                   mock_delegate_.GetWeakPtr(), test_form_id(),
                   test_field_id());

  ASSERT_EQ(controller_.suggestions().size(), 2u);
  EXPECT_EQ(controller_.suggestions()[0].main_text.value, u"John Doe");
  EXPECT_EQ(controller_.suggestions()[1].main_text.value, u"Jane Doe");
  EXPECT_EQ(controller_.GetMainFillingProduct(), FillingProduct::kAddress);
}

// Tests that `Hide()` informs the delegate and emits metrics.
TEST_F(AutofillSuggestionControllerIOSTest, HideNotifiesDelegate) {
  EXPECT_CALL(mock_delegate_, GetMainFillingProduct())
      .WillRepeatedly(Return(FillingProduct::kAddress));

  controller_.Show(CreateTestSuggestions(),
                   AutofillSuggestionTriggerSource::kFormControlElementClicked,
                   mock_delegate_.GetWeakPtr(), test_form_id(),
                   test_field_id());

  EXPECT_CALL(mock_delegate_,
              OnSuggestionsHidden(SuggestionHidingReason::kAcceptSuggestion));

  controller_.Hide(SuggestionHidingReason::kAcceptSuggestion);

  EXPECT_FALSE(controller_.is_showing());
  EXPECT_TRUE(controller_.suggestions().empty());
  histogram_tester_.ExpectUniqueSample(
      "Autofill.PopupHidingReason.Address",
      SuggestionHidingReason::kAcceptSuggestion, 1);
}

// Tests that calling `Hide()` when not showing is a safe no-op.
TEST_F(AutofillSuggestionControllerIOSTest, HideWhenNotShowingIsNoOp) {
  EXPECT_CALL(mock_delegate_, OnSuggestionsHidden(_)).Times(0);

  controller_.Hide(SuggestionHidingReason::kUserAborted);
}

// Tests that the Undo suggestion is moved first on both `Show()` and
// `UpdateSuggestions()`.
TEST_F(AutofillSuggestionControllerIOSTest, UndoSuggestionOrderedFirst) {
  std::vector<Suggestion> suggestions = CreateTestSuggestions();
  suggestions.emplace_back(u"Undo", SuggestionType::kUndo);

  controller_.Show(
      suggestions, AutofillSuggestionTriggerSource::kFormControlElementClicked,
      mock_delegate_.GetWeakPtr(), test_form_id(), test_field_id());
  EXPECT_THAT(
      controller_.suggestions(),
      ElementsAre(Field(&Suggestion::type, SuggestionType::kUndo),
                  Field(&Suggestion::type, SuggestionType::kAddressEntry),
                  Field(&Suggestion::type, SuggestionType::kAddressEntry)));
  EXPECT_EQ(controller_.suggestions()[1].main_text.value, u"John Doe");

  controller_.UpdateSuggestions(
      suggestions, FillingProduct::kAddress,
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
  EXPECT_EQ(controller_.suggestions()[0].type, SuggestionType::kUndo);
  EXPECT_EQ(controller_.suggestions()[1].main_text.value, u"John Doe");
}

// Tests that `Show()` with empty suggestions hides the showing suggestions.
TEST_F(AutofillSuggestionControllerIOSTest, ShowWithoutSuggestionsHides) {
  controller_.Show(CreateTestSuggestions(),
                   AutofillSuggestionTriggerSource::kFormControlElementClicked,
                   mock_delegate_.GetWeakPtr(), test_form_id(),
                   test_field_id());

  EXPECT_CALL(mock_delegate_, OnSuggestionsShown).Times(0);
  EXPECT_CALL(mock_delegate_,
              OnSuggestionsHidden(SuggestionHidingReason::kNoSuggestions));
  controller_.Show(
      {}, AutofillSuggestionTriggerSource::kFormControlElementClicked,
      mock_delegate_.GetWeakPtr(), test_form_id(), test_field_id());

  EXPECT_FALSE(controller_.is_showing());
  EXPECT_EQ(controller_.GetUiSessionId(), std::nullopt);
}

// Tests that `UpdateSuggestions()` with empty suggestions hides the showing
// suggestions.
TEST_F(AutofillSuggestionControllerIOSTest,
       UpdateSuggestionsWithoutSuggestionsHides) {
  controller_.Show(CreateTestSuggestions(),
                   AutofillSuggestionTriggerSource::kFormControlElementClicked,
                   mock_delegate_.GetWeakPtr(), test_form_id(),
                   test_field_id());

  EXPECT_CALL(mock_delegate_,
              OnSuggestionsHidden(SuggestionHidingReason::kNoSuggestions));
  controller_.UpdateSuggestions(
      {}, FillingProduct::kAddress,
      AutofillSuggestionTriggerSource::kFormControlElementClicked);

  EXPECT_FALSE(controller_.is_showing());
  EXPECT_EQ(controller_.GetUiSessionId(), std::nullopt);
}

// Tests that `UpdateSuggestions()` hides the showing suggestions with
// `kStaleData` when `main_filling_product` does not match the showing product.
TEST_F(AutofillSuggestionControllerIOSTest,
       UpdateSuggestionsProductMismatchHidesAsStale) {
  controller_.Show(CreateTestSuggestions(),
                   AutofillSuggestionTriggerSource::kFormControlElementClicked,
                   mock_delegate_.GetWeakPtr(), test_form_id(),
                   test_field_id());

  EXPECT_CALL(mock_delegate_,
              OnSuggestionsHidden(SuggestionHidingReason::kStaleData));
  controller_.UpdateSuggestions(
      {Suggestion(u"4111", SuggestionType::kCreditCardEntry)},
      FillingProduct::kCreditCard,
      AutofillSuggestionTriggerSource::kFormControlElementClicked);

  EXPECT_FALSE(controller_.is_showing());
  EXPECT_EQ(controller_.GetUiSessionId(), std::nullopt);
}

// Tests that `UpdateSuggestions()` notifies the delegate of the updated
// suggestions while keeping the same session.
TEST_F(AutofillSuggestionControllerIOSTest, UpdateSuggestionsNotifiesDelegate) {
  controller_.Show(CreateTestSuggestions(),
                   AutofillSuggestionTriggerSource::kFormControlElementClicked,
                   mock_delegate_.GetWeakPtr(), test_form_id(),
                   test_field_id());
  const std::optional<AutofillSuggestionControllerIOS::UiSessionId> session_id =
      controller_.GetUiSessionId();

  std::vector<Suggestion> updated_suggestions = {
      Suggestion(u"Jane Doe", SuggestionType::kAddressEntry)};
  EXPECT_CALL(
      mock_delegate_,
      OnSuggestionsShown(
          ElementsAre(Field(&Suggestion::main_text,
                            Field(&Suggestion::Text::value, u"Jane Doe"))),
          _));
  controller_.UpdateSuggestions(
      updated_suggestions, FillingProduct::kAddress,
      AutofillSuggestionTriggerSource::kFormControlElementClicked);

  EXPECT_TRUE(controller_.is_showing());
  EXPECT_EQ(controller_.GetUiSessionId(), session_id);
}

// Tests that showing suggestions for another field closes the session of the
// showing suggestions on its delegate before starting the new one.
TEST_F(AutofillSuggestionControllerIOSTest,
       ShowForAnotherFieldClosesPreviousSession) {
  const FieldGlobalId first_field_id = MainFrameFieldId(FieldRendererId(1));
  controller_.Show(CreateTestSuggestions(),
                   AutofillSuggestionTriggerSource::kFormControlElementClicked,
                   mock_delegate_.GetWeakPtr(), FormGlobalId(), first_field_id);

  {
    InSequence s;
    EXPECT_CALL(mock_delegate_,
                OnSuggestionsHidden(SuggestionHidingReason::kViewDestroyed));
    EXPECT_CALL(mock_delegate_, OnSuggestionsShown(_, _));
  }
  const FieldGlobalId other_field_id = MainFrameFieldId(FieldRendererId(2));
  controller_.Show(CreateTestSuggestions(),
                   AutofillSuggestionTriggerSource::kFormControlElementClicked,
                   mock_delegate_.GetWeakPtr(), FormGlobalId(), other_field_id);

  EXPECT_TRUE(controller_.is_showing());
  EXPECT_EQ(controller_.field_id(), other_field_id);
  // The controller hides the showing suggestions on destruction.
  testing::Mock::VerifyAndClearExpectations(&mock_delegate_);
}

// Tests that `IsShowingForFrame()` matches both the field's frame and its
// form's frame.
TEST_F(AutofillSuggestionControllerIOSTest, IsShowingForFrame) {
  const LocalFrameToken main_frame_token(
      *DeserializeJavaScriptFrameId(web::kMainFakeFrameId));
  const LocalFrameToken child_frame_token(
      *DeserializeJavaScriptFrameId(web::kChildFakeFrameId));
  const LocalFrameToken other_child_frame_token(
      *DeserializeJavaScriptFrameId(web::kChildFakeFrameId2));
  EXPECT_FALSE(controller_.IsShowingForFrame(main_frame_token));
  EXPECT_FALSE(controller_.IsShowingForFrame(LocalFrameToken()));

  controller_.Show(CreateTestSuggestions(),
                   AutofillSuggestionTriggerSource::kFormControlElementClicked,
                   mock_delegate_.GetWeakPtr(), FormGlobalId(),
                   MainFrameFieldId(FieldRendererId(1)));

  EXPECT_TRUE(controller_.IsShowingForFrame(main_frame_token));
  EXPECT_FALSE(controller_.IsShowingForFrame(child_frame_token));
  EXPECT_FALSE(controller_.IsShowingForFrame(LocalFrameToken()));

  controller_.Hide(SuggestionHidingReason::kUserAborted);
  EXPECT_FALSE(controller_.IsShowingForFrame(main_frame_token));

  // A field in a child frame whose form is hosted by the main frame matches
  // both frames, and no other frame.
  controller_.Show(CreateTestSuggestions(),
                   AutofillSuggestionTriggerSource::kFormControlElementClicked,
                   mock_delegate_.GetWeakPtr(),
                   FormGlobalId(main_frame_token, FormRendererId(1)),
                   FieldGlobalId(child_frame_token, FieldRendererId(1)));

  EXPECT_TRUE(controller_.IsShowingForFrame(main_frame_token));
  EXPECT_TRUE(controller_.IsShowingForFrame(child_frame_token));
  EXPECT_FALSE(controller_.IsShowingForFrame(other_child_frame_token));
  EXPECT_FALSE(controller_.IsShowingForFrame(LocalFrameToken()));
}

}  // namespace
}  // namespace autofill
