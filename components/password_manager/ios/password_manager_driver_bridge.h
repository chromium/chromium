// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_DRIVER_BRIDGE_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_DRIVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include <string>

#include "url/origin.h"

namespace autofill {
struct PasswordFormFillData;
struct PasswordFormGenerationData;
}  // namespace autofill

class GURL;

// C++ to ObjC bridge for methods of PasswordManagerDriver.
@protocol PasswordManagerDriverBridge

@property(readonly, nonatomic) const GURL& lastCommittedURL;

// Prepares fill data with given password form data.
// This method calls suggestions helper's processWithPasswordFormFillData.
- (void)processPasswordFormFillData:
            (const autofill::PasswordFormFillData&)formData
                         forFrameId:(const std::string&)frameId
                        isMainFrame:(BOOL)isMainFrame
                  forSecurityOrigin:(const url::Origin&)origin;

// Informs delegate that there are no saved credentials for the current page.
// The frame is used to get the AccountSelectFillData and reset the credentials
// cache and also to detach the bottom sheet listener.
- (void)onNoSavedCredentialsWithFrameId:(const std::string&)frameId;

// Informs delegate of form for password generation found.
- (void)formEligibleForGenerationFound:
    (const autofill::PasswordFormGenerationData&)form;

- (void)attachListenersForPasswordGenerationFields:
            (const autofill::PasswordFormGenerationData&)form
                                        forFrameId:(const std::string&)frameId;

// Scrolls the field into view and checks if its view area is visible in the
// frame.
- (void)scrollAndCheckViewAreaVisible:(autofill::FieldRendererId)fieldId
                           forFrameId:(const std::string&)frameId
                    completionHandler:(void (^)(BOOL visible))completionHandler;

// Fills the triggering field with the given value in the specified frame.
- (void)fillField:(autofill::FieldRendererId)fieldId
            withValue:(const std::u16string&)value
           forFrameId:(const std::string&)frameId
    completionHandler:(void (^)(BOOL success))completionHandler;

@end

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_DRIVER_BRIDGE_H_
