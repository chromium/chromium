// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_SUGGESTION_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_SUGGESTION_HELPER_H_

#import <Foundation/Foundation.h>
#include <memory>

#include "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#include "url/gurl.h"

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
class WebFrame;
class WebState;
}  // namespace web

// A protocol implemented by a delegate of PasswordSuggestionHelper.
@protocol PasswordSuggestionHelperDelegate<NSObject>

// Called when form extraction is required for checking suggestion availability.
// The caller must trigger the form extraction in this method.
- (void)suggestionHelperShouldTriggerFormExtraction:
            (PasswordSuggestionHelper*)suggestionHelper
                                            inFrame:(web::WebFrame*)frame;

@end

// Provides common logic of password autofill suggestions for both ios/chrome
// and ios/web_view.
// TODO(crbug.com/1097353): Consider folding this class into
// SharedPasswordController.
@interface PasswordSuggestionHelper : NSObject

// Delegate to receive callbacks.
@property(nonatomic, weak) id<PasswordSuggestionHelperDelegate> delegate;

// Creates a instance with the given |webState|.
- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Retrieves suggestions as username and realm pairs
// (defined in |password_manager::UsernameAndRealm|) and converts
// them into objective C representations. In the returned |FormSuggestion|
// items, |value| field will be the username and |displayDescription| will be
// the realm.
- (NSArray<FormSuggestion*>*)
    retrieveSuggestionsWithFormID:(autofill::FormRendererId)formIdentifier
                  fieldIdentifier:(autofill::FieldRendererId)fieldIdentifier
                          inFrame:(web::WebFrame*)frame
                        fieldType:(NSString*)fieldType;

// Checks if suggestions are available for the field.
// |completion| will be called when the check is completed, with boolean
// parameter indicating whether suggestions are available or not.
// See //components/autofill/ios/form_util/form_activity_params.h for definition
// of other parameters.
- (void)checkIfSuggestionsAvailableForForm:
            (FormSuggestionProviderQuery*)formQuery
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion;

// Retrieves password form fill data for |username| for use in
// |PasswordFormHelper|'s
// -fillPasswordFormWithFillData:completionHandler:.
- (std::unique_ptr<password_manager::FillData>)
    passwordFillDataForUsername:(NSString*)username
                        inFrame:(web::WebFrame*)frame;

// The following methods should be called to maintain the correct state along
// with password forms.

// Resets fill data, callbacks and state flags for new page. This method should
// be called in password controller's -webState:didFinishNavigation:.
- (void)resetForNewPage;

// Prepares fill data with given password form data. Triggers callback for
// -checkIfSuggestionsAvailableForForm... if needed.
// This method should be called in password controller's
// -processPasswordFormFillData.
- (void)processWithPasswordFormFillData:
            (const autofill::PasswordFormFillData&)formData
                                inFrame:(web::WebFrame*)frame
                            isMainFrame:(BOOL)isMainFrame
                      forSecurityOrigin:(const GURL&)origin;

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
