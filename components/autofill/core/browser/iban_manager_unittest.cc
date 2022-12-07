// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/iban_manager.h"

#include "base/guid.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/suggestions_context.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Field;
using testing::Truly;
using testing::UnorderedElementsAre;

namespace autofill {

namespace {

class MockSuggestionsHandler : public IBANManager::SuggestionsHandler {
 public:
  MockSuggestionsHandler() = default;
  MockSuggestionsHandler(const MockSuggestionsHandler&) = delete;
  MockSuggestionsHandler& operator=(const MockSuggestionsHandler&) = delete;
  ~MockSuggestionsHandler() override = default;

  MOCK_METHOD(void,
              OnSuggestionsReturned,
              (FieldGlobalId field_id,
               AutoselectFirstSuggestion autoselect_first_suggestion,
               const std::vector<Suggestion>& suggestions),
              (override));

  base::WeakPtr<MockSuggestionsHandler> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockSuggestionsHandler> weak_ptr_factory_{this};
};

}  // namespace

class IBANManagerTest : public testing::Test {
 protected:
  IBANManagerTest()
      : iban_manager_(&personal_data_manager_, /*is_off_the_record=*/false) {}

  // Sets up the TestPersonalDataManager with an IBAN.
  IBAN SetUpIBAN(base::StringPiece16 value, base::StringPiece16 nickname) {
    IBAN iban;
    std::string guid = base::GenerateGUID();
    iban.set_guid(guid);
    iban.set_value(std::u16string(value));
    iban.set_nickname(std::u16string(nickname));
    personal_data_manager_.AddIBANForTest(std::make_unique<IBAN>(iban));
    return iban;
  }

  // Sets up the TestPersonalDataManager with an IBAN and corresponding
  // suggestion.
  Suggestion SetUpIBANAndSuggestion(base::StringPiece16 value,
                                    base::StringPiece16 nickname) {
    IBAN iban = SetUpIBAN(value, nickname);
    Suggestion iban_suggestion(iban.GetIdentifierStringForAutofillDisplay());
    return iban_suggestion;
  }

  base::test::TaskEnvironment task_environment_;
  MockSuggestionsHandler suggestions_handler_;
  TestAutofillClient autofill_client_;
  TestPersonalDataManager personal_data_manager_;
  IBANManager iban_manager_;
};

TEST_F(IBANManagerTest, ShowsIBANSuggestions) {
  Suggestion iban_suggestion_0 =
      SetUpIBANAndSuggestion(u"IE12 BOFI 9000 0112 3456 78", u"Nickname 0");
  Suggestion iban_suggestion_1 =
      SetUpIBANAndSuggestion(u"CH56 0483 5012 3456 7800 9", u"Nickname 1");

  SuggestionsContext context;
  FormFieldData test_field;

  // Setting up mock to verify that the handler is returned a list of
  // iban-based suggestions and the iban details line.
  EXPECT_CALL(
      suggestions_handler_,
      OnSuggestionsReturned(
          test_field.global_id(), AutoselectFirstSuggestion(false),
          UnorderedElementsAre(
              Field(&Suggestion::main_text, iban_suggestion_0.main_text),
              Field(&Suggestion::main_text, iban_suggestion_1.main_text))))
      .Times(1);

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(),
      /*context=*/context));
}

TEST_F(IBANManagerTest, ShowsIBANSuggestions_OnlyPrefixMatch) {
  base::StringPiece16 value_0 = u"IE12 BOFI 9000 0112 3456 78";
  Suggestion iban_suggestion_0 = SetUpIBANAndSuggestion(value_0, u"Nickname 0");
  Suggestion iban_suggestion_1 =
      SetUpIBANAndSuggestion(u"CH56 0483 5012 3456 7800 9", u"Nickname 1");

  SuggestionsContext context;
  FormFieldData test_field;
  test_field.value = std::u16string(value_0);

  // Setting up mock to verify that the handler is not returned any iban-based
  // suggestions as the field already contains an iban.
  EXPECT_CALL(suggestions_handler_,
              OnSuggestionsReturned(
                  _, _,
                  testing::Truly(
                      [](const std::vector<Suggestion>& returned_suggestions) {
                        return returned_suggestions.empty();
                      })));

  // Simulate request for suggestions.
  // Because all criteria are met to trigger returning to the handler,
  // the handler should be triggered and this should return true.
  EXPECT_TRUE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(),
      /*context=*/context));
}

TEST_F(IBANManagerTest, DoesNotShowIBANsForOffTheRecord) {
  IBAN iban_0 = SetUpIBAN(u"IE12 BOFI 9000 0112 3456 78", u"Nickname 0");
  iban_manager_.SetOffTheRecordForTesting(true);
  SuggestionsContext context;
  FormFieldData test_field;

  // Setting up mock to verify that suggestions returning is not triggered if
  // the user is off the record.
  EXPECT_CALL(suggestions_handler_, OnSuggestionsReturned).Times(0);

  // Simulate request for suggestions.
  EXPECT_FALSE(iban_manager_.OnGetSingleFieldSuggestions(
      AutoselectFirstSuggestion(false), test_field, autofill_client_,
      suggestions_handler_.GetWeakPtr(),
      /*context=*/context));
}

}  // namespace autofill
