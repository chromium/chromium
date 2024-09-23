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

// Delegate to perform tasks outside of PasswordSuggestionHelper.
@protocol PasswordSuggestionHelperDelegate<NSObject>

// Called when form extraction is required for checking suggestion availability.
// The caller must trigger the form extraction in this method.
- (void)suggestionHelperShouldTriggerFormExtraction:
            (PasswordSuggestionHelper*)suggestionHelper
                                            inFrame:(web::WebFrame*)frame;

// Adds event listeners to fields which are associated with a bottom sheet.
// When the focus event occurs on these fields, a bottom sheet will be shown
// instead of the keyboard, allowing the user to fill the fields by tapping
// one of the suggestions.
- (void)attachListenersForBottomSheet:
            (const std::vector<autofill::FieldRendererId>&)rendererIds
                           forFrameId:(const std::string&)frameId;

@end

// Processes and provides password suggestions for form fields across frames
// within a specific web state. Also a hub for password filling signals (e.g.
// track focus on field).
// TODO(crbug.com/40701292): Consider folding this class into
// SharedPasswordController.
@interface PasswordSuggestionHelper : NSObject

// Delegate to receive callbacks.
@property(nonatomic, weak) id<PasswordSuggestionHelperDelegate> delegate;

// Creates a instance for the given |webState|.
- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Retrieves suggestions for a field within a form from `formQuery`.
- (NSArray<FormSuggestion*>*)retrieveSuggestionsWithForm:
    (FormSuggestionProviderQuery*)formQuery;

// Checks if suggestions are available for the field within a form targeted by
// the provided |formQuery|. Calls |completion| on completion with a boolean
// parameter indicating the availability of suggestions.
//
// The |completion| callback will be called asynchronously if it has to try
// extracting password forms from the renderer when there is no fill data
// at the moment of the check (in an attempt to retrieve fill data in the case
// the forms in the renderer were dynamically updated).
- (void)checkIfSuggestionsAvailableForForm:
            (FormSuggestionProviderQuery*)formQuery
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion;

// Retrieves password form fill data for `frameId` and `username`.
- (std::unique_ptr<password_manager::FillData>)
    passwordFillDataForUsername:(NSString*)username
                     forFrameId:(const std::string&)frameId;

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
                             forFrameId:(const std::string&)frameId
                            isMainFrame:(BOOL)isMainFrame
                      forSecurityOrigin:(const GURL&)origin;

// Processes the case in which no saved credentials are available when
// extracting forms in the renderer. Will complete the pending check query
// with the available fill data at that moment (likely empty but there might be
// asynchronous server suggestions that were processed in the meantime; between
// the moment extraction started and completion).
- (void)processWithNoSavedCredentialsWithFrameId:(const std::string&)frameId;

// Returns NO if the field does not have the "password" field type or if the
// field is confirmed to NOT be a password field by server predictions.
// Returns YES otherwise.
- (BOOL)isPasswordFieldOnForm:(FormSuggestionProviderQuery*)formQuery
                     webFrame:(web::WebFrame*)webFrame;

@end

NS_ASSUME_NONNULL_END

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_SUGGESTION_HELPER_H_
