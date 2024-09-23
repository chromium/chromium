// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_PROVIDER_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_PROVIDER_H_

#include "components/autofill/core/browser/filling_product.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider_query.h"

@protocol FormSuggestionProvider;

namespace web {
class WebState;
}  // namespace web

typedef void (^SuggestionsAvailableCompletion)(BOOL suggestionsAvailable);
typedef void (^SuggestionsReadyCompletion)(
    NSArray<FormSuggestion*>* suggestions,
    id<FormSuggestionProvider> delegate);
typedef void (^SuggestionHandledCompletion)(void);

// Different types for providers.
typedef NS_ENUM(NSUInteger, SuggestionProviderType) {
  SuggestionProviderTypeUnknown,
  SuggestionProviderTypePassword,
  SuggestionProviderTypeAutofill,
};

// Provides user-selectable suggestions for an input field of a web form
// and handles user interaction with those suggestions.
@protocol FormSuggestionProvider<NSObject>

// The type of the suggestion provider.
@property(nonatomic, readonly) SuggestionProviderType type;

// Main type of the shown suggestions.
@property(nonatomic, readonly) autofill::FillingProduct mainFillingProduct;

// Determines whether the receiver can provide suggestions for the specified
// |form| and |field|, returning the result using the provided |completion|.
// |typedValue| contains the text that the user has typed into the field so far.
// TODO(crbug.com/40128249): Remove formName and fieldIdentifier once unique IDs
// are used in Autofill.
- (void)checkIfSuggestionsAvailableForForm:
            (FormSuggestionProviderQuery*)formQuery
                            hasUserGesture:(BOOL)hasUserGesture
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion;

// Retrieves suggestions for the specified |form| and |field| and returns them
// using the provided |completion|. |typedValue| contains the text that the
// user has typed into the field so far.
- (void)retrieveSuggestionsForForm:(FormSuggestionProviderQuery*)formQuery
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion;

// Handles user selection of a suggestion at |index| for the specified form and
// field, invoking |completion| when finished.
- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                    atIndex:(NSInteger)index
                       form:(NSString*)formName
             formRendererID:(autofill::FormRendererId)formRendererID
            fieldIdentifier:(NSString*)fieldIdentifier
            fieldRendererID:(autofill::FieldRendererId)fieldRendererID
                    frameID:(NSString*)frameID
          completionHandler:(SuggestionHandledCompletion)completion;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_PROVIDER_H_
