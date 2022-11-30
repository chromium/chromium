// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_FAKE_AUTOFILL_AGENT_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_FAKE_AUTOFILL_AGENT_H_

#import <Foundation/Foundation.h>

#import "components/autofill/ios/browser/autofill_agent.h"

NS_ASSUME_NONNULL_BEGIN

@class FormSuggestion;

// Autofill agent used in tests. Use to stub out suggestion fetching by
// providing them using in-memory data.
@interface FakeAutofillAgent : AutofillAgent

// Adds a |suggestion| to be returned for form with |formName| and a field
// with |fieldIdentifier|.
- (void)addSuggestion:(FormSuggestion*)suggestion
          forFormName:(NSString*)formName
      fieldIdentifier:(NSString*)fieldIdentifier
              frameID:(NSString*)frameID;

// Returns the last selected |suggestion| for form with |formName| and field
// with |fieldIdentifier|.
- (FormSuggestion*)selectedSuggestionForFormName:(NSString*)formName
                                 fieldIdentifier:(NSString*)fieldIdentifier
                                         frameID:(NSString*)frameID;

@end

NS_ASSUME_NONNULL_END

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_FAKE_AUTOFILL_AGENT_H_
