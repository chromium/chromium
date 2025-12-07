// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/plus_addresses/plus_address_suggestion_generator.h"

#include <memory>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/plus_addresses/mock_autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/suggestions/plus_addresses/plus_address.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {
using ::base::test::RunOnceCallback;
using ::base::test::RunOnceCallbackRepeatedly;
using ::base::test::TestFuture;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Pair;
using ::testing::Return;
using ::testing::SaveArg;
using SuggestionData = SuggestionGenerator::SuggestionData;
using ReturnedSuggestions = SuggestionGenerator::ReturnedSuggestions;
using ProductSuggestionDataPair =
    std::pair<SuggestionGenerator::SuggestionDataSource,
              std::vector<SuggestionData>>;

Matcher<std::vector<Suggestion>> IsSingleFillPlusAddressSuggestion(
    std::string_view address) {
  return ElementsAre(
      AllOf(EqualsSuggestion(SuggestionType::kFillExistingPlusAddress,
                             /*main_text=*/base::UTF8ToUTF16(address)),
            Field(&Suggestion::icon, Suggestion::Icon::kPlusAddress)));
}

class PlusAddressSuggestionGeneratorTest : public testing::Test {
 public:
  PlusAddressSuggestionGeneratorTest() {
    form_ = test::CreateTestPersonalInformationFormData();
    form_structure_ = std::make_unique<FormStructure>(form_);
    test_api(*form_structure_)
        .SetFieldTypes({NAME_FIRST, NAME_MIDDLE, NAME_LAST, EMAIL_ADDRESS});
    autofill_field_ = form_structure_->field(3);

    ON_CALL(plus_address_delegate_, IsFieldEligibleForPlusAddress)
        .WillByDefault(Return(true));

    ON_CALL(plus_address_delegate_, GetSuggestionsFromPlusAddresses)
        .WillByDefault([](const std::vector<std::string>& plus_addresses) {
          std::vector<Suggestion> suggestions;
          suggestions.reserve(plus_addresses.size());

          for (const auto& plus_address : plus_addresses) {
            suggestions.emplace_back(plus_address, "",
                                     Suggestion::Icon::kPlusAddress,
                                     SuggestionType::kFillExistingPlusAddress);
          }

          return suggestions;
        });
  }

  FormData& form() { return form_; }
  FormFieldData& field() { return *autofill_field_; }
  const FormStructure* form_structure() { return form_structure_.get(); }
  const AutofillField* autofill_field() { return autofill_field_; }
  const AutofillClient& client() { return test_autofill_client_; }

  std::vector<Suggestion> FetchAndGenerateSuggestions(
      PlusAddressSuggestionGenerator& generator,
      FieldType field_type) {
    test_api(*form_structure_)
        .SetFieldTypes({UNKNOWN_TYPE, UNKNOWN_TYPE, UNKNOWN_TYPE, field_type});

    base::MockOnceCallback<void(ProductSuggestionDataPair)> fetch_cb;
    ProductSuggestionDataPair fetched_suggestions;
    EXPECT_CALL(fetch_cb, Run).WillOnce(SaveArg<0>(&fetched_suggestions));
    generator.FetchSuggestionData(form(), field(), form_structure(),
                                  autofill_field(), client(), fetch_cb.Get());

    TestFuture<ReturnedSuggestions> future;
    generator.GenerateSuggestions(
        form(), field(), form_structure(), autofill_field(), client(),
        {{fetched_suggestions}}, future.GetCallback());
    return future.Take().second;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment test_environment_;
  TestAutofillClient test_autofill_client_;
  MockAutofillPlusAddressDelegate plus_address_delegate_;
  FormData form_;
  std::unique_ptr<FormStructure> form_structure_;
  // Owned by `form_structure_`.
  raw_ptr<AutofillField> autofill_field_ = nullptr;
};

TEST_F(PlusAddressSuggestionGeneratorTest, NoPlusAddressesSaved) {
  const std::vector<std::string> plus_addresses = {};
  EXPECT_CALL(plus_address_delegate_, IsPlusAddressFillingEnabled)
      .WillOnce(Return(true));
  EXPECT_CALL(plus_address_delegate_, GetAffiliatedPlusAddresses)
      .WillOnce(RunOnceCallback<1>(plus_addresses));

  PlusAddressSuggestionGenerator generator(&plus_address_delegate_,
                                           /*is_manually_triggered=*/false);

  base::MockOnceCallback<void(ProductSuggestionDataPair)> fetch_cb;
  ProductSuggestionDataPair fetched_suggestions;
  EXPECT_CALL(fetch_cb, Run).WillOnce(SaveArg<0>(&fetched_suggestions));
  generator.FetchSuggestionData(form(), field(), form_structure(),
                                autofill_field(), client(), fetch_cb.Get());

  EXPECT_EQ(fetched_suggestions.first,
            SuggestionGenerator::SuggestionDataSource::kPlusAddress);
  EXPECT_THAT(fetched_suggestions.second, IsEmpty());
}

// Tests that fill plus address suggestions are offered iff the value in the
// focused field matches the prefix of an existing plus address.
TEST_F(PlusAddressSuggestionGeneratorTest, SuggestionsForExistingPlusAddress) {
  const std::vector<std::string> plus_addresses = {"plus+remote@plus.plus"};
  EXPECT_CALL(plus_address_delegate_, IsPlusAddressFillingEnabled)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(plus_address_delegate_, GetAffiliatedPlusAddresses)
      .WillRepeatedly(RunOnceCallbackRepeatedly<1>(plus_addresses));
  PlusAddressSuggestionGenerator generator(&plus_address_delegate_,
                                           /*is_manually_triggered=*/false);

  // We offer filling if the field is empty.
  EXPECT_THAT(FetchAndGenerateSuggestions(generator, EMAIL_ADDRESS),
              IsSingleFillPlusAddressSuggestion(plus_addresses[0]));

  // If the user types a letter and it matches the plus address (after
  // normalization), the plus address continues to be offered.
  field().set_value(u"P");
  EXPECT_THAT(FetchAndGenerateSuggestions(generator, EMAIL_ADDRESS),
              IsSingleFillPlusAddressSuggestion(plus_addresses[0]));

  // If the value does not match the prefix of the plus address, nothing is
  // shown.
  field().set_value(u"pp");
  EXPECT_THAT(FetchAndGenerateSuggestions(generator, EMAIL_ADDRESS), IsEmpty());
}

// Tests that fill plus address suggestions regardless of whether there is
// already text in the field if the trigger source was manual fallback.
TEST_F(PlusAddressSuggestionGeneratorTest,
       SuggestionsForExistingPlusAddressWithManualFallback) {
  const std::vector<std::string> plus_addresses = {"plus+remote@plus.plus"};
  EXPECT_CALL(plus_address_delegate_, GetAffiliatedPlusAddresses)
      .WillRepeatedly(RunOnceCallbackRepeatedly<1>(plus_addresses));

  test_api(*form_structure_)
      .SetFieldTypes({UNKNOWN_TYPE, UNKNOWN_TYPE, UNKNOWN_TYPE, UNKNOWN_TYPE});
  PlusAddressSuggestionGenerator generator(&plus_address_delegate_,
                                           /*is_manually_triggered=*/true);

  // We offer filling if the field is empty.
  EXPECT_THAT(FetchAndGenerateSuggestions(generator, UNKNOWN_TYPE),
              IsSingleFillPlusAddressSuggestion(plus_addresses[0]));

  // We also offer filling if the field is not empty and the prefix does not
  // match the address.
  field().set_value(u"pp");
  EXPECT_THAT(FetchAndGenerateSuggestions(generator, UNKNOWN_TYPE),
              IsSingleFillPlusAddressSuggestion(plus_addresses[0]));
}

// Tests that no suggestions are returned when plus address are disabled.
TEST_F(PlusAddressSuggestionGeneratorTest, NoSuggestionsWhenDisabled) {
  const std::vector<std::string> plus_addresses = {"plus+remote@plus.plus"};
  EXPECT_CALL(plus_address_delegate_, IsPlusAddressFillingEnabled)
      .WillOnce(Return(false));

  PlusAddressSuggestionGenerator generator(&plus_address_delegate_,
                                           /*is_manually_triggered=*/false);

  EXPECT_THAT(FetchAndGenerateSuggestions(generator, EMAIL_ADDRESS), IsEmpty());
}

}  // namespace
}  // namespace autofill
