// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_AGENT_H
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_AGENT_H

#import <Foundation/Foundation.h>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/common/form_data_predictions.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

namespace autofill {
class AutofillPopupDelegate;
struct FormData;
}

class PrefService;

namespace web {
class WebState;
}

// Handles autofill form suggestions. Reads forms from the page, sends them to
// AutofillManager for metrics and to retrieve suggestions, and fills forms in
// response to user interaction with suggestions. This is the iOS counterpart
// to the upstream class autofill::AutofillAgent.
@interface AutofillAgent : NSObject<FormSuggestionProvider>

// Designated initializer. Arguments |prefService| and |webState| should not be
// null.
- (instancetype)initWithPrefService:(PrefService*)prefService
                           webState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Callback by AutofillController when suggestions are ready.
- (void)onSuggestionsReady:(NSArray<FormSuggestion*>*)suggestions
             popupDelegate:
                 (const base::WeakPtr<autofill::AutofillPopupDelegate>&)
                     delegate;

// The supplied data should be filled into the form in |frame|.
- (void)onFormDataFilled:(const autofill::FormData&)result
                 inFrame:(web::WebFrame*)frame;

// Detatches from the web state.
- (void)detachFromWebState;

// Renders the field type predictions specified in |forms| in |frame|. This
// method is a no-op if the kAutofillShowTypePredictions experiment is not
// enabled.
- (void)renderAutofillTypePredictions:
            (const std::vector<autofill::FormDataPredictions>&)forms
                              inFrame:(web::WebFrame*)frame;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_AGENT_H
