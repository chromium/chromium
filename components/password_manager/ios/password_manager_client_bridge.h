// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_CLIENT_BRIDGE_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_CLIENT_BRIDGE_H_

#import <Foundation/Foundation.h>
#include <memory>

#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"

class GURL;

namespace password_manager {
class PasswordFormManagerForUI;
class PasswordManager;
}  // namespace password_manager

namespace web {
class WebState;
}  // namespace web

using password_manager::CredentialLeakType;

// C++ to ObjC bridge for methods of PasswordManagerClient.
@protocol PasswordManagerClientBridge

@property(readonly, nonatomic) web::WebState* webState;

@property(readonly, nonatomic)
    password_manager::PasswordManager* passwordManager;

@property(readonly, nonatomic) const GURL& lastCommittedURL;

// Shows UI to prompt the user to save the password.
- (void)showSavePasswordInfoBar:
            (std::unique_ptr<password_manager::PasswordFormManagerForUI>)
                formToSave
                         manual:(BOOL)manual;

// Shows UI to prompt the user to update the password.
- (void)showUpdatePasswordInfoBar:
            (std::unique_ptr<password_manager::PasswordFormManagerForUI>)
                formToUpdate
                           manual:(BOOL)manual;

// Removes the saving/updating password Infobar from the InfobarManager.
// This also causes the UI to be dismissed.
- (void)removePasswordInfoBarManualFallback:(BOOL)manual;

// Shows Password Breach for |URL| and |leakType|.
- (void)showPasswordBreachForLeakType:(CredentialLeakType)leakType
                                  URL:(const GURL&)URL;

// Shows Password Protection warning with |warningText|.
- (void)showPasswordProtectionWarning:(NSString*)warningText;

@end

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_CLIENT_BRIDGE_H_
