// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_BRIDGE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_BRIDGE_H_

#include <vector>

#import "base/functional/callback_forward.h"
#import "base/memory/weak_ptr.h"
#import "components/autofill/core/common/unique_ids.h"

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

// Shows a snackbar that offers a user to undo filling a plus address as part of
// address form filling.
- (void)showPlusAddressEmailOverrideNotification:
    (base::OnceClosure)emailOverrideUndoCallback;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_BRIDGE_H_
