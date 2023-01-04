// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_CLIENT_BRIDGE_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_CLIENT_BRIDGE_H_

#import <Foundation/Foundation.h>
#include <memory>
#include <string>

#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"

class GURL;
enum class CredentialProviderPromoTrigger;

namespace password_manager {
class PasswordFormManagerForUI;
class PasswordManager;
}  // namespace password_manager

namespace safe_browsing {
enum class WarningAction;
}  // namespace safe_browsing

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

// Shows Password Breach for |URL|, |leakType|, and |username|.
- (void)showPasswordBreachForLeakType:(CredentialLeakType)leakType
                                  URL:(const GURL&)URL
                             username:(const std::u16string&)username;

// Shows Password Protection warning with |warningText|. |completion| should be
// called when the UI is dismissed with the user's |action|.
- (void)showPasswordProtectionWarning:(NSString*)warningText
                           completion:(void (^)(safe_browsing::WarningAction))
                                          completion;

// Shows Credential Provider Promo with |trigger|.
- (void)showCredentialProviderPromo:(CredentialProviderPromoTrigger)trigger;

@end

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_MANAGER_CLIENT_BRIDGE_H_
