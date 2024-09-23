// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_FORM_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_FORM_HELPER_H_

#import <Foundation/Foundation.h>

#include "base/memory/ref_counted_memory.h"
#include "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "url/gurl.h"

NS_ASSUME_NONNULL_BEGIN

@class PasswordFormHelper;

namespace autofill {
class FormData;
struct PasswordFormFillData;
}  // namespace autofill

namespace password_manager {
struct FillData;
}  // namespace password_manager

namespace web {
class WebFrame;
class WebState;
}  // namespace web

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Status returned after handling the submitted form. Any item that isn't
// kHandled represents a rejection where the submission wasn't handled.
enum class HandleSubmittedFormStatus {
  kHandled = 0,
  kRejectedNoWebState = 1,
  kRejectedNoDelegate = 2,
  kRejectedMessageBodyNotADict = 3,
  kRejectedNoFrameMatchingId = 4,
  kRejectedNoTrustedUrl = 5,
  kRejectedCantExtractFormData = 6,
  kMaxValue = kRejectedCantExtractFormData
};

// A protocol implemented by a delegate of PasswordFormHelper.
@protocol PasswordFormHelperDelegate
// Called when the password form is submitted.
- (void)formHelper:(PasswordFormHelper*)formHelper
     didSubmitForm:(const autofill::FormData&)form
           inFrame:(web::WebFrame*)frame;
@end

// Handles common form processing logic of password controller for both
// ios/chrome and ios/web_view.
// TODO(crbug.com/40701292): Consider folding this class into
// SharedPasswordController.
@interface PasswordFormHelper
    : NSObject<FormActivityObserver, CRWWebStateObserver>

// Last committed URL of current web state.
// Returns empty URL if current web state is not available.
@property(nonatomic, readonly) const GURL& lastCommittedURL;

// The associated delegate.
@property(nonatomic, nullable, weak) id<PasswordFormHelperDelegate> delegate;

// Uses JavaScript to find password forms. Calls |completionHandler| with the
// extracted information used for matching and saving passwords. Calls
// |completionHandler| with an empty vector if no password forms are found.
- (void)findPasswordFormsInFrame:(web::WebFrame*)frame
               completionHandler:
                   (nullable void (^)(const std::vector<autofill::FormData>&))
                       completionHandler;

// Fills new password field for (optional as @"") |newPasswordIdentifier| and
// for (optional as @"") confirm password field |confirmPasswordIdentifier| in
// the form identified by |formData|. Invokes |completionHandler| with true if
// any fields were filled, false otherwise.
- (void)fillPasswordForm:(autofill::FormRendererId)formIdentifier
                      inFrame:(web::WebFrame*)frame
        newPasswordIdentifier:(autofill::FieldRendererId)newPasswordIdentifier
    confirmPasswordIdentifier:
        (autofill::FieldRendererId)confirmPasswordIdentifier
            generatedPassword:(NSString*)generatedPassword
            completionHandler:(nullable void (^)(BOOL))completionHandler;

// Autofills credentials into the page on credential suggestion selection.
// Credentials and input fields are specified by |fillData|. |fieldRendererID|
// specifies the unput on which the suggestion was accepted. Invokes
// |completionHandler| when finished with YES if successful and NO otherwise.
- (void)fillPasswordFormWithFillData:(password_manager::FillData)fillData
                             inFrame:(web::WebFrame*)frame
                    triggeredOnField:(autofill::FieldRendererId)fieldRendererID
                   completionHandler:(nullable void (^)(BOOL))completionHandler;

// Finds the password form with unique ID |formIdentifier| and calls
// |completionHandler| with the populated |FormData| data structure. |found| is
// YES if the current form was found successfully, NO otherwise.
- (void)extractPasswordFormData:(autofill::FormRendererId)formIdentifier
                        inFrame:(web::WebFrame*)frame
              completionHandler:
                  (void (^)(BOOL found,
                            const autofill::FormData& form))completionHandler;

// Updates the stored field data. In case if there is a presaved credential it
// updates the presaved credential.
- (void)updateFieldDataOnUserInput:(autofill::FieldRendererId)field_id
                           inFrame:(web::WebFrame*)frame
                        inputValue:(NSString*)field_value;

// Processes `message` sent by JavaScript to the `PasswordFormSubmitButtonClick`
// handler.
- (HandleSubmittedFormStatus)handleFormSubmittedMessage:
    (const web::ScriptMessage&)message;

// Creates a instance with the given |webState|.
- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_FORM_HELPER_H_
