// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_FORM_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_FORM_HELPER_H_

#import <Foundation/Foundation.h>

#include "base/memory/ref_counted_memory.h"
#include "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "url/gurl.h"

NS_ASSUME_NONNULL_BEGIN

@class PasswordFormHelper;

namespace autofill {
class FieldDataManager;
struct FormData;
struct PasswordFormFillData;
}  // namespace autofill

namespace password_manager {
struct FillData;
// Returns true if the trust level for the current page URL of |web_state| is
// kAbsolute. If |page_url| is not null, fills it with the current page URL.
bool GetPageURLAndCheckTrustLevel(web::WebState* web_state,
                                  GURL* __nullable page_url);
}  // namespace password_manager

namespace web {
class WebState;
}  // namespace web

// A protocol implemented by a delegate of PasswordFormHelper.
@protocol PasswordFormHelperDelegate
// Called when the password form is submitted.
- (void)formHelper:(PasswordFormHelper*)formHelper
     didSubmitForm:(const autofill::FormData&)form
       inMainFrame:(BOOL)inMainFrame;
@end

// Handles common form processing logic of password controller for both
// ios/chrome and ios/web_view.
// TODO(crbug.com/1097353): Consider folding this class into
// SharedPasswordController.
@interface PasswordFormHelper
    : NSObject<FormActivityObserver, CRWWebStateObserver>

// Last committed URL of current web state.
// Returns empty URL if current web state is not available.
@property(nonatomic, readonly) const GURL& lastCommittedURL;

// Maps UniqueFieldId of an input element to the pair of:
// 1) The most recent text that user typed or PasswordManager autofilled in
// input elements. Used for storing username/password before JavaScript
// changes them.
// 2) Field properties mask, i.e. whether the field was autofilled, modified
// by user, etc. (see FieldPropertiesMask).
@property(nonatomic, readonly) scoped_refptr<autofill::FieldDataManager>
    fieldDataManager;

// The associated delegate.
@property(nonatomic, nullable, weak) id<PasswordFormHelperDelegate> delegate;

// Uses JavaScript to find password forms. Calls |completionHandler| with the
// extracted information used for matching and saving passwords. Calls
// |completionHandler| with an empty vector if no password forms are found.
- (void)findPasswordFormsWithCompletionHandler:
    (nullable void (^)(const std::vector<autofill::FormData>&,
                       uint32_t))completionHandler;

// Autofills credentials into the page. Credentials and input fields are
// specified by |formData|. Invokes |completionHandler| when finished with YES
// if successful and NO otherwise.
- (void)fillPasswordForm:(const autofill::PasswordFormFillData&)formData
       completionHandler:(nullable void (^)(BOOL))completionHandler;

// Fills new password field for (optional as @"") |newPasswordIdentifier| and
// for (optional as @"") confirm password field |confirmPasswordIdentifier| in
// the form identified by |formData|. Invokes |completionHandler| with true if
// any fields were filled, false otherwise.
- (void)fillPasswordForm:(autofill::FormRendererId)formIdentifier
        newPasswordIdentifier:(autofill::FieldRendererId)newPasswordIdentifier
    confirmPasswordIdentifier:
        (autofill::FieldRendererId)confirmPasswordIdentifier
            generatedPassword:(NSString*)generatedPassword
            completionHandler:(nullable void (^)(BOOL))completionHandler;

// Autofills credentials into the page on credential suggestion selection.
// Credentials and input fields are specified by |fillData|. |uniqueFieldID|
// specifies the unput on which the suggestion was accepted. Invokes
// |completionHandler| when finished with YES if successful and NO otherwise.
- (void)fillPasswordFormWithFillData:(const password_manager::FillData&)fillData
                    triggeredOnField:(autofill::FieldRendererId)uniqueFieldID
                   completionHandler:(nullable void (^)(BOOL))completionHandler;

// Finds the password form with unique ID |formIdentifier| and calls
// |completionHandler| with the populated |FormData| data structure. |found| is
// YES if the current form was found successfully, NO otherwise.
- (void)extractPasswordFormData:(autofill::FormRendererId)formIdentifier
              completionHandler:
                  (void (^)(BOOL found,
                            const autofill::FormData& form))completionHandler;

// Sets up the next available numeric value for setting unique renderer ids
// in |frame|.
- (void)setUpForUniqueIDsWithInitialState:(uint32_t)nextAvailableID
                                  inFrame:(web::WebFrame*)frame;

// Updates the stored field data. In case if there is a presaved credential it
// updates the presaved credential.
- (void)updateFieldDataOnUserInput:(autofill::FieldRendererId)field_id
                        inputValue:(NSString*)field_value;

// Creates a instance with the given |webState|.
- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_FORM_HELPER_H_
