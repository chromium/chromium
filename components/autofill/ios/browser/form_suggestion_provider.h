// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_PROVIDER_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_PROVIDER_H_

#import "components/autofill/ios/browser/form_suggestion.h"

@protocol FormSuggestionProvider;

namespace web {
class WebState;
}  // namespace web

typedef void (^SuggestionsAvailableCompletion)(BOOL suggestionsAvailable);
typedef void (^SuggestionsReadyCompletion)(
    NSArray<FormSuggestion*>* suggestions,
    id<FormSuggestionProvider> delegate);
typedef void (^SuggestionHandledCompletion)(void);

// Provides user-selectable suggestions for an input field of a web form
// and handles user interaction with those suggestions.
@protocol FormSuggestionProvider<NSObject>

// Determines whether the receiver can provide suggestions for the specified
// |form| and |field|, returning the result using the provided |completion|.
// |typedValue| contains the text that the user has typed into the field so far.
- (void)checkIfSuggestionsAvailableForForm:(NSString*)formName
                           fieldIdentifier:(NSString*)fieldIdentifier
                                 fieldType:(NSString*)fieldType
                                      type:(NSString*)type
                                typedValue:(NSString*)typedValue
                                   frameID:(NSString*)frameID
                               isMainFrame:(BOOL)isMainFrame
                            hasUserGesture:(BOOL)hasUserGesture
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion;

// Retrieves suggestions for the specified |form| and |field| and returns them
// using the provided |completion|. |typedValue| contains the text that the
// user has typed into the field so far.
- (void)retrieveSuggestionsForForm:(NSString*)formName
                   fieldIdentifier:(NSString*)fieldIdentifier
                         fieldType:(NSString*)fieldType
                              type:(NSString*)type
                        typedValue:(NSString*)typedValue
                           frameID:(NSString*)frameID
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion;

// Handles user selection of a suggestion for the specified form and
// field, invoking |completion| when finished.
- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                       form:(NSString*)formName
            fieldIdentifier:(NSString*)fieldIdentifier
                    frameID:(NSString*)frameID
          completionHandler:(SuggestionHandledCompletion)completion;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_PROVIDER_H_
