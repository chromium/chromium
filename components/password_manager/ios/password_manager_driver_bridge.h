// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_DRIVER_BRIDGE_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_DRIVER_BRIDGE_H_

#import <Foundation/Foundation.h>

namespace autofill {
struct PasswordFormFillData;
struct PasswordFormGenerationData;
}  // namespace autofill

namespace web {
class WebFrame;
}  // namespace web

class GURL;

// C++ to ObjC bridge for methods of PasswordManagerDriver.
@protocol PasswordManagerDriverBridge

@property(readonly, nonatomic) const GURL& lastCommittedURL;

// Prepares fill data with given password form data.
// This method calls suggestions helper's processWithPasswordFormFillData.
- (void)processPasswordFormFillData:
            (const autofill::PasswordFormFillData&)formData
                            inFrame:(web::WebFrame*)frame
                        isMainFrame:(BOOL)isMainFrame
                  forSecurityOrigin:(const GURL&)origin;

// Informs delegate that there are no saved credentials for the current page.
- (void)onNoSavedCredentialsWithFrame:(web::WebFrame*)frame;

// Informs delegate of form for password generation found.
- (void)formEligibleForGenerationFound:
    (const autofill::PasswordFormGenerationData&)form;

@end

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_DRIVER_BRIDGE_H_
