// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_SELECTION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_SELECTION_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ui/suggestion.h"

namespace autofill {

class AutofillProfile;
class AutofillProfileComparator;
class AutofillType;

namespace suggestion_selection {

extern const size_t kMaxSuggestedProfilesCount;
extern const size_t kMaxUniqueSuggestionsCount;
extern const size_t kMaxPrunedUniqueSuggestionsCount;

// Normalizes text for comparison based on the type of the field `text` was
// entered into.
std::u16string NormalizeForComparisonForType(const std::u16string& text,
                                             ServerFieldType type);

// Matches based on prefix search, and limits number of profiles.
// Returns the top matching suggestions based on prefix search, and adds the
// corresponding profiles to |matched_profiles|. At most
// |kMaxSuggestedProfilesCount| are returned.
std::vector<Suggestion> GetPrefixMatchedSuggestions(
    const AutofillType& type,
    const std::u16string& raw_field_contents,
    const std::u16string& field_contents_canon,
    const std::string& app_locale,
    bool field_is_autofilled,
    const std::vector<AutofillProfile*>& profiles,
    std::vector<AutofillProfile*>* matched_profiles);

// Dedupes given suggestions based on if one is a subset of the other.
// Returns unique_suggestions, and adds the corresponding profiles to
// |unique_matched_profiles|. At most |kMaxUniqueSuggestionsCount| are returned.
// |field_types| stores all of the form's ServerFieldTypes, including that of
// the field on which the user is currently focused.
std::vector<Suggestion> GetUniqueSuggestions(
    const ServerFieldTypeSet& field_types,
    const AutofillProfileComparator& comparator,
    const std::string app_locale,
    const std::vector<AutofillProfile*> matched_profiles,
    const std::vector<Suggestion>& suggestions,
    std::vector<AutofillProfile*>* unique_matched_profiles);

// Returns whether the |suggestion_canon| is valid considering the
// |field_contents_canon|, the |type|, |is_masked_server_card|, and
// |field_is_autofilled|. Assigns true to |is_prefix_matched| if the
// |field_contents_canon| is a prefix to |suggestion_canon|, assigns false
// otherwise.
bool IsValidSuggestionForFieldContents(std::u16string suggestion_canon,
                                       std::u16string field_contents_canon,
                                       const AutofillType& type,
                                       bool is_masked_server_card,
                                       bool field_is_autofilled,
                                       bool* is_prefix_matched);

// Removes profiles that haven't been used after |min_last_used| from
// |profiles|. The relative ordering of |profiles| is maintained.
void RemoveProfilesNotUsedSinceTimestamp(
    base::Time min_last_used,
    std::vector<AutofillProfile*>* profiles);

// Prepares a collection of Suggestions to show to the user. Adds |labels| to
// their corresponding |suggestions| and removes duplicates, if any. A label
// corresponds to the suggestion with the same index.
//
// NOTE: |suggestions| are assumed to have already been sorted from most to
// least important.
void PrepareSuggestions(const std::vector<std::u16string>& labels,
                        std::vector<Suggestion>* suggestions,
                        const AutofillProfileComparator& comparator);

}  // namespace suggestion_selection
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_SELECTION_H_
