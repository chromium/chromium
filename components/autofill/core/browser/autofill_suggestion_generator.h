// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SUGGESTION_GENERATOR_H_

#include <vector>

namespace base {
class Time;
}  // namespace base

namespace autofill {

class AutofillClient;
class AutofillType;
class CreditCard;
struct FormFieldData;
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
  // autofill_client_ and the generator are both one per tab, and have the same
  // lifecycle.
  AutofillClient* autofill_client_;

  // personal_data_ should outlive the generator.
  PersonalDataManager* personal_data_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_SUGGESTION_GENERATOR_H_
