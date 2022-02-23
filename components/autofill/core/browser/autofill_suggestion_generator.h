// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SUGGESTION_GENERATOR_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"

namespace base {
class Time;
}  // namespace base

namespace autofill {

class AutofillClient;
class AutofillType;
class CreditCard;
struct FormFieldData;
class FormStructure;
class PersonalDataManager;
struct Suggestion;

// Helper class to generate Autofill suggestions, such as for credit card and
// address profile Autofill.
class AutofillSuggestionGenerator {
 public:
  explicit AutofillSuggestionGenerator(AutofillClient* autofill_client,
                                       PersonalDataManager* personal_data);
  ~AutofillSuggestionGenerator() = default;
  AutofillSuggestionGenerator(const AutofillSuggestionGenerator&) = delete;
  AutofillSuggestionGenerator& operator=(const AutofillSuggestionGenerator&) =
      delete;

  // Generates suggestions for all available credit cards.
  std::vector<Suggestion> GetSuggestionsForCreditCards(
      const FormStructure& form_structure,
      const FormFieldData& field,
      const AutofillType& type,
      const std::string& app_locale);

  // Remove credit cards that are expired at |comparison_time| and not used
  // since |min_last_used| from |cards|. The relative ordering of |cards| is
  // maintained.
  static void RemoveExpiredCreditCardsNotUsedSinceTimestamp(
      base::Time comparison_time,
      base::Time min_last_used,
      std::vector<CreditCard*>* cards);

  // Return a nickname for the |card| to display. This is generally the nickname
  // stored in |card|, unless |card| exists as a local and a server copy. In
  // this case, we prefer the nickname of the local if it is defined. If only
  // one copy has a nickname, take that.
  std::u16string GetDisplayNicknameForCreditCard(const CreditCard& card) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(AutofillSuggestionGeneratorTest,
                           CreateCreditCardSuggestion_LocalCard);
  FRIEND_TEST_ALL_PREFIXES(AutofillSuggestionGeneratorTest,
                           CreateCreditCardSuggestion_ServerCard);
  FRIEND_TEST_ALL_PREFIXES(AutofillSuggestionGeneratorTest,
                           GetServerCardForLocalCard);
  FRIEND_TEST_ALL_PREFIXES(AutofillSuggestionGeneratorTest,
                           ShouldShowVirtualCardOption);

  // Creates a suggestion for the given |credit_card|. |type| denotes the
  // AutofillType of the field that is focused when the query is triggered.
  // |prefix_matched_suggestion| indicates whether the suggestion has content
  // that prefix-matches the field content. |virtual_card_option| suggests
  // whether the suggestion is a virtual card option.
  Suggestion CreateCreditCardSuggestion(const CreditCard& credit_card,
                                        const AutofillType& type,
                                        bool prefix_matched_suggestion,
                                        bool virtual_card_option,
                                        const std::string& app_locale) const;

  // Helper function to decide whether to show the virtual card option for
  // |candidate_card| given the |form_structure|.
  bool ShouldShowVirtualCardOption(const CreditCard* candidate_card,
                                   const FormStructure& form_structure) const;

  // Returns a pointer to the server card that has duplicate information of the
  // |local_card|. It is not guaranteed that a server card is found. If not,
  // nullptr is returned.
  const CreditCard* GetServerCardForLocalCard(
      const CreditCard* local_card) const;

  // autofill_client_ and the generator are both one per tab, and have the same
  // lifecycle.
  raw_ptr<AutofillClient> autofill_client_;

  // personal_data_ should outlive the generator.
  raw_ptr<PersonalDataManager> personal_data_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SUGGESTION_GENERATOR_H_
