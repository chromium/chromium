// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_BRIDGE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_BRIDGE_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {
class AutofillSuggestionDelegate;
struct Suggestion;
}

// Interface used to pipe events from AutofillClientIOS to the embedder.
@protocol AutofillClientIOSBridge

- (void)showAutofillPopup:(const std::vector<autofill::Suggestion>&)suggestions
       suggestionDelegate:
           (const base::WeakPtr<autofill::AutofillSuggestionDelegate>&)delegate;

- (void)hideAutofillPopup;

// Checks whether the qurrent query is the most recent one.
- (bool)isLastQueriedField:(autofill::FieldGlobalId)fieldId;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_BRIDGE_H_
