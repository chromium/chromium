// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_SELECTION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_SELECTION_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ui/suggestion.h"

namespace autofill {

class AutofillProfile;
class AutofillProfileComparator;
class AutofillType;

namespace suggestion_selection {

extern const size_t kMaxSuggestedProfilesCount;
extern const size_t kMaxUniqueSuggestedProfilesCount;

// Sets the `popup_item_id` for `suggestion` depending on
// `last_filling_granularity`. If the `last_filling_granularity` for a certain
// form was group filling, also add labels to give users feedback about the next
// filling behaviour.
// `last_targeted_fields` specified the last set of fields target by the user.
// When not present, we default to full form.
// TODO(crbug.com/1466116): Add tests when this is actually used.
// TODO(crbug.com/1466116): Add labels when `last_filling_granularity` is group
// filling.
void AddSuggestionDetailsForCurrentFillingGranularity(
    absl::optional<ServerFieldTypeSet> last_targeted_fields,
    const AutofillType& triggering_field_type,
    Suggestion& suggestion);

// Creates nested/child suggestions for `suggestion` with the `profile`
// information. Uses `type` to define what group filling suggestion to add
// (name, address or phone). The existence of child suggestions defines whether
// the autofill popup will have submenus.
// `last_targeted_fields` specified the last set of fields target by the user.
// When not present, we default to full form.
void AddGranularFillingChildSuggestions(
    const AutofillType& type,
    absl::optional<ServerFieldTypeSet> last_targeted_fields,
    const AutofillProfile& profile,
    const std::string& app_locale,
    Suggestion& suggestion);

// In addition to just getting the values out of the autocomplete profile, this
// function handles formatting of the street addresses and phone numbers.
std::u16string GetSuggestionMainText(const AutofillProfile* profile,
                                     const AutofillType& type,
                                     const std::string& app_locale);

// Normalizes text for comparison based on the type of the field `text` was
// entered into.
std::u16string NormalizeForComparisonForType(const std::u16string& text,
                                             ServerFieldType type);

// Matches based on prefix search, and limits number of profiles.
// Returns the top matching profiles based on prefix search. At most
// `kMaxSuggestedProfilesCount` are returned.
std::vector<AutofillProfile*> GetPrefixMatchedProfiles(
    const AutofillType& type,
    const std::u16string& raw_field_contents,
    const std::u16string& field_contents_canon,
    const std::string& app_locale,
    bool field_is_autofilled,
    const std::vector<AutofillProfile*>& profiles);

// Dedupes the given profiles based on if one is a subset of the other for
// suggestions represented by `field_types`. The function returns at most
// `kMaxUniqueSuggestedProfilesCount` profiles. `field_types` stores all of the
// ServerFieldTypes relevant for the current suggestions, including that of the
// field on which the user is currently focused.
std::vector<AutofillProfile*> DeduplicatedProfilesForSuggestions(
    const AutofillType& type,
    const ServerFieldTypeSet& field_types,
    const AutofillProfileComparator& comparator,
    const std::vector<AutofillProfile*> matched_profiles);

// Returns whether the |suggestion_canon| is valid considering the
// |field_contents_canon|, the |type|, |is_masked_server_card|, and
// |field_is_autofilled|.
bool IsValidSuggestionForFieldContents(std::u16string suggestion_canon,
                                       std::u16string field_contents_canon,
                                       const AutofillType& type,
                                       bool is_masked_server_card,
                                       bool field_is_autofilled);

// Removes profiles that haven't been used after |min_last_used| from
// |profiles|. The relative ordering of |profiles| is maintained.
void RemoveProfilesNotUsedSinceTimestamp(
    base::Time min_last_used,
    std::vector<AutofillProfile*>* profiles);

}  // namespace suggestion_selection
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_SELECTION_H_
