// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_BRIDGE_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_BRIDGE_H_

#include <vector>

#include "base/memory/weak_ptr.h"

namespace autofill {
class AutofillPopupDelegate;
struct Suggestion;
}

// Interface used to pipe events from AutofillClientIOS to the embedder.
@protocol AutofillClientIOSBridge

- (void)showAutofillPopup:(const std::vector<autofill::Suggestion>&)suggestions
            popupDelegate:
                (const base::WeakPtr<autofill::AutofillPopupDelegate>&)delegate;

- (void)hideAutofillPopup;

// Checks whether the qurrent query is the most recent one.
- (bool)isQueryIDRelevant:(int)queryID;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_CLIENT_IOS_BRIDGE_H_
