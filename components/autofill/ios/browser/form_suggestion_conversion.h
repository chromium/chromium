// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_CONVERSION_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_CONVERSION_H_

#import <Foundation/Foundation.h>

#import <vector>

#import "base/memory/weak_ptr.h"
#import "components/autofill/core/common/unique_ids.h"

@class FormSuggestion;

namespace autofill {

class AutofillClient;
class AutofillSuggestionDelegate;
struct Suggestion;

// Converts the Autofill `popup_suggestion` at `index` in its source list into a
// keyboard accessory `FormSuggestion`. `index` is stored as
// `FormSuggestionMetadata::popup_suggestion_index` so that the suggestion can
// be accepted by index. Returns nil if `popup_suggestion` can't be displayed in
// the keyboard accessory (e.g. headers, separators, footers).
FormSuggestion* FormSuggestionFromAutofillSuggestion(
    const Suggestion& popup_suggestion,
    size_t index,
    const base::WeakPtr<AutofillSuggestionDelegate>& delegate,
    FormGlobalId form_id,
    FieldGlobalId field_id);

// Converts `suggestions` into keyboard accessory `FormSuggestion`s using
// `FormSuggestionFromAutofillSuggestion`. The Undo suggestion is moved to the
// front and, if `client` is non-null and allows it, an AtMemory search button
// (without `popup_suggestion_index`) is appended to a non-empty result.
NSArray<FormSuggestion*>* FormSuggestionsFromAutofillSuggestions(
    const std::vector<Suggestion>& suggestions,
    const base::WeakPtr<AutofillSuggestionDelegate>& delegate,
    FormGlobalId form_id,
    FieldGlobalId field_id,
    const AutofillClient* client);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_CONVERSION_H_
