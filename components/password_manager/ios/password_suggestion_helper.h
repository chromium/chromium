// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_SUGGESTION_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_SUGGESTION_HELPER_H_

#import <Foundation/Foundation.h>
#include <memory>

#import "components/autofill/ios/browser/form_suggestion_provider.h"

NS_ASSUME_NONNULL_BEGIN

@class FormSuggestion;
@class PasswordSuggestionHelper;

namespace autofill {
struct PasswordFormFillData;
}  // namespace autofill

namespace password_manager {
struct FillData;
}  // namespace password_manager

namespace web {
class WebState;
}  // namespace web

// A protocol implemented by a delegate of PasswordSuggestionHelper.
@protocol PasswordSuggestionHelperDelegate<NSObject>

// Called when form extraction is required for checking suggestion availability.
// The caller must trigger the form extraction in this method.
- (void)suggestionHelperShouldTriggerFormExtraction:
    (PasswordSuggestionHelper*)suggestionHelper;

@end

// Provides common logic of password autofill suggestions for both ios/chrome
// and ios/web_view.
@interface PasswordSuggestionHelper : NSObject

// Creates an instance with the given delegate.
- (instancetype)initWithDelegate:(id<PasswordSuggestionHelperDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Retrieves suggestions as username and realm pairs
// (defined in |password_manager::UsernameAndRealm|) and converts
// them into objective C representations. In the returned |FormSuggestion|
// items, |value| field will be the username and |displayDescription| will be
// the realm.
- (NSArray<FormSuggestion*>*)
retrieveSuggestionsWithFormName:(NSString*)formName
                fieldIdentifier:(NSString*)fieldIdentifier
                      fieldType:(NSString*)fieldType;

// Checks if suggestions are available for the field.
// |completion| will be called when the check is completed, with boolean
// parameter indicating whether suggestions are available or not.
// See //components/autofill/ios/form_util/form_activity_params.h for definition
// of other parameters.
- (void)checkIfSuggestionsAvailableForForm:(NSString*)formName
                           fieldIdentifier:(NSString*)fieldIdentifier
                                 fieldType:(NSString*)fieldType
                                      type:(NSString*)type
                                   frameID:(NSString*)frameID
                               isMainFrame:(BOOL)isMainFrame
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion;

// Retrieves password form fill data for |username| for use in
// |PasswordFormHelper|'s
// -fillPasswordFormWithFillData:completionHandler:.
- (std::unique_ptr<password_manager::FillData>)getFillDataForUsername:
    (NSString*)username;

// The following methods should be called to maintain the correct state along
// with password forms.

// Resets fill data, callbacks and state flags for new page. This method should
// be called in password controller's -webState:didLoadPageWithSuccess:.
- (void)resetForNewPage;

// Prepares fill data with given password form data. Triggers callback for
// -checkIfSuggestionsAvailableForForm... if needed.
// This method should be called in password controller's
// -fillPasswordForm:completionHandler:.
- (void)processWithPasswordFormFillData:
    (const autofill::PasswordFormFillData&)formData;

// Processes field for which no saved credentials are available.
// Triggers callback for -checkIfSuggestionsAvailableForForm... if needed.
// This method should be called in password controller's
// -onNoSavedCredentials.
- (void)processWithNoSavedCredentials;

// Updates the state for password form extraction state.
// This method should be called in password controller's
// -didFinishPasswordFormExtraction:, when the extracted forms are not empty.
- (void)updateStateOnPasswordFormExtracted;

@end

NS_ASSUME_NONNULL_END

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_SUGGESTION_HELPER_H_
