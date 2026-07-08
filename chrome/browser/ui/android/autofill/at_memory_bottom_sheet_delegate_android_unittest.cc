// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/at_memory_bottom_sheet_delegate_android.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/ui/mock_autofill_suggestion_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::NiceMock;

class AtMemoryBottomSheetDelegateAndroidTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  TestAutofillClient client_;
  NiceMock<MockAutofillSuggestionDelegate> mock_suggestion_delegate_;
};

TEST_F(AtMemoryBottomSheetDelegateAndroidTest, OnDismissedHidesSuggestions) {
  AtMemoryBottomSheetDelegateAndroid delegate(
      &client_, mock_suggestion_delegate_.GetWeakPtr(), /*suggestions=*/{});
  ON_CALL(mock_suggestion_delegate_, GetMainFillingProduct)
      .WillByDefault(testing::Return(FillingProduct::kAtMemory));
  client_.ShowAutofillSuggestions(AutofillClient::PopupOpenArgs(),
                                  mock_suggestion_delegate_.GetWeakPtr());

  delegate.OnDismissed();

  EXPECT_EQ(client_.popup_hiding_reason(),
            SuggestionHidingReason::kUserAborted);
}

TEST_F(AtMemoryBottomSheetDelegateAndroidTest, OnQuerySubmittedCallsDelegate) {
  AtMemoryBottomSheetDelegateAndroid delegate(
      &client_, mock_suggestion_delegate_.GetWeakPtr(), /*suggestions=*/{});

  EXPECT_CALL(mock_suggestion_delegate_,
              OnSearchSubmitted(std::u16string(u"query")));
  delegate.OnQuerySubmitted(u"query");
}

TEST_F(AtMemoryBottomSheetDelegateAndroidTest,
       OnSuggestionSelectedCallsDelegate) {
  std::vector<Suggestion> suggestions = {
      Suggestion(u"first", SuggestionType::kAddressEntry),
      Suggestion(u"second", SuggestionType::kAddressEntry)};
  AtMemoryBottomSheetDelegateAndroid delegate(
      &client_, mock_suggestion_delegate_.GetWeakPtr(), suggestions);

  EXPECT_CALL(
      mock_suggestion_delegate_,
      DidAcceptSuggestion(
          suggestions[1],
          AutofillSuggestionDelegate::SuggestionMetadata{.multi_index = {1}}));
  delegate.OnSuggestionSelected(1);
}

TEST_F(AtMemoryBottomSheetDelegateAndroidTest,
       OnChildSuggestionSelectedCallsDelegate) {
  Suggestion child0(u"child0", SuggestionType::kAddressEntry);
  Suggestion child1(u"child1", SuggestionType::kAddressEntry);
  Suggestion parent0(u"parent0", SuggestionType::kAddressEntry);
  Suggestion parent1(u"parent1", SuggestionType::kAddressEntry);
  parent1.children = {child0, child1};
  std::vector<Suggestion> suggestions = {parent0, parent1};
  AtMemoryBottomSheetDelegateAndroid delegate(
      &client_, mock_suggestion_delegate_.GetWeakPtr(), suggestions);

  EXPECT_CALL(mock_suggestion_delegate_,
              DidAcceptSuggestion(
                  child1, AutofillSuggestionDelegate::SuggestionMetadata{
                              .multi_index = {1, 1}}));
  delegate.OnChildSuggestionSelected(1, 1);
}

TEST_F(AtMemoryBottomSheetDelegateAndroidTest,
       OnChildSuggestionsShownCallsDelegate) {
  Suggestion child0(u"child0", SuggestionType::kAddressEntry);
  Suggestion child1(u"child1", SuggestionType::kAddressEntry);
  Suggestion parent0(u"parent0", SuggestionType::kAddressEntry);
  Suggestion parent1(u"parent1", SuggestionType::kAddressEntry);
  parent1.children = {child0, child1};
  std::vector<Suggestion> suggestions = {parent0, parent1};
  AtMemoryBottomSheetDelegateAndroid delegate(
      &client_, mock_suggestion_delegate_.GetWeakPtr(), suggestions);

  const AutofillSuggestionDelegate::SuggestionMetadata expected_metadata{
      .multi_index = {1}};
  EXPECT_CALL(mock_suggestion_delegate_,
              OnSuggestionsShown(ElementsAreArray(parent1.children),
                                 Eq(expected_metadata)));
  delegate.OnChildSuggestionsShown(1);
}

}  // namespace autofill
