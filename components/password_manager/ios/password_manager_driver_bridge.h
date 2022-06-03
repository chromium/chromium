// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_DRIVER_BRIDGE_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_DRIVER_BRIDGE_H_

#import <Foundation/Foundation.h>

namespace autofill {
struct PasswordFormFillData;
struct PasswordFormGenerationData;
}  // namespace autofill

class GURL;

namespace password_manager {
class PasswordGenerationFrameHelper;
}  // namespace password_manager

// C++ to ObjC bridge for methods of PasswordManagerDriver.
@protocol PasswordManagerDriverBridge

@property(readonly, nonatomic) const GURL& lastCommittedURL;

// Finds and fills the password form using the supplied |formData| to
// match the password form and to populate the field values. Calls
// |completionHandler| with YES if a form field has been filled, NO otherwise.
// |completionHandler| can be nil.
- (void)fillPasswordForm:(const autofill::PasswordFormFillData&)formData
       completionHandler:(void (^)(BOOL))completionHandler;

// Informs delegate that there are no saved credentials for the current page.
- (void)onNoSavedCredentials;

// Gets the PasswordGenerationFrameHelper owned by this delegate.
- (password_manager::PasswordGenerationFrameHelper*)passwordGenerationHelper;

// Informs delegate of form for password generation found.
- (void)formEligibleForGenerationFound:
    (const autofill::PasswordFormGenerationData&)form;

@end

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_DRIVER_BRIDGE_H_
