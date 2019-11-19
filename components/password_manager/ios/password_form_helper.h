// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_FORM_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_FORM_HELPER_H_

#import <Foundation/Foundation.h>

#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "url/gurl.h"

NS_ASSUME_NONNULL_BEGIN

@class JsPasswordManager;
@class PasswordFormHelper;

namespace autofill {
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
@interface PasswordFormHelper
    : NSObject<FormActivityObserver, CRWWebStateObserver>

// The JsPasswordManager processing password form via javascript.
@property(nonatomic, readonly) JsPasswordManager* jsPasswordManager;

// Last committed URL of current web state.
// Returns empty URL if current web state is not available.
@property(nonatomic, readonly) const GURL& lastCommittedURL;

// Uses JavaScript to find password forms. Calls |completionHandler| with the
// extracted information used for matching and saving passwords. Calls
// |completionHandler| with an empty vector if no password forms are found.
- (void)findPasswordFormsWithCompletionHandler:
    (nullable void (^)(const std::vector<autofill::FormData>&))
        completionHandler;

// Autofills credentials into the page. Credentials and input fields are
// specified by |formData|. Invokes |completionHandler| when finished with YES
// if successful and NO otherwise.
- (void)fillPasswordForm:(const autofill::PasswordFormFillData&)formData
       completionHandler:(nullable void (^)(BOOL))completionHandler;

// Fills new password field for (optional as @"") |newPasswordIdentifier| and
// for (optional as @"") confirm password field |confirmPasswordIdentifier| in
// the form identified by |formData|. Invokes |completionHandler| with true if
// any fields were filled, false otherwise.
- (void)fillPasswordForm:(NSString*)formName
        newPasswordIdentifier:(NSString*)newPasswordIdentifier
    confirmPasswordIdentifier:(NSString*)confirmPasswordIdentifier
            generatedPassword:(NSString*)generatedPassword
            completionHandler:(nullable void (^)(BOOL))completionHandler;

// Autofills credentials into the page. Credentials and input fields are
// specified by |fillData|. Invokes |completionHandler| when finished with YES
// if successful and NO otherwise.
- (void)fillPasswordFormWithFillData:(const password_manager::FillData&)fillData
                   completionHandler:(nullable void (^)(BOOL))completionHandler;

// Finds password forms in the page and fills them with the |username| and
// |password|. If not nil, |completionHandler| is called once per form filled.
- (void)findAndFillPasswordFormsWithUserName:(NSString*)username
                                    password:(NSString*)password
                           completionHandler:
                               (nullable void (^)(BOOL))completionHandler;

// Finds the password form named |formName| and calls
// |completionHandler| with the populated |FormData| data structure. |found| is
// YES if the current form was found successfully, NO otherwise.
- (void)extractPasswordFormData:(NSString*)formName
              completionHandler:
                  (void (^)(BOOL found,
                            const autofill::FormData& form))completionHandler;

// Creates a instance with the given WebState, observer and delegate.
- (instancetype)initWithWebState:(web::WebState*)webState
                        delegate:
                            (nullable id<PasswordFormHelperDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_FORM_HELPER_H_
