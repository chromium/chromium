// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_SHARED_PASSWORD_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_SHARED_PASSWORD_CONTROLLER_H_

#import <Foundation/Foundation.h>
#include <memory>

#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/form_util/form_activity_observer.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#import "components/password_manager/ios/password_form_helper.h"
#import "components/password_manager/ios/password_manager_driver_bridge.h"
#import "components/password_manager/ios/password_suggestion_helper.h"
#import "ios/web/public/web_state_observer_bridge.h"

namespace password_manager {
class PasswordManagerClient;
class PasswordManagerDriver;
}  // namespace password_manager

@class FormSuggestion;
@class SharedPasswordController;

// Protocol to define methods that must be implemented by the embedder.
@protocol SharedPasswordControllerDelegate <NSObject>

// The PasswordManagerClient owned by the delegate.
@property(nonatomic, readonly)
    password_manager::PasswordManagerClient* passwordManagerClient;

// The PasswordManagerClient owned by the delegate.
@property(nonatomic, readonly)
    password_manager::PasswordManagerDriver* passwordManagerDriver;

// Called to inform the delegate that it should prompt the user for a decision
// on whether or not to use the |generatedPotentialPassword|.
// |decisionHandler| takes a single BOOL indicating if the user accepted the
// the suggested |generatedPotentialPassword|.
- (void)sharedPasswordController:(SharedPasswordController*)controller
    showGeneratedPotentialPassword:(NSString*)generatedPotentialPassword
                   decisionHandler:(void (^)(BOOL accept))decisionHandler;

// Called when SharedPasswordController accepts a suggestion displayed to the
// user. This can be used to additional client specific behavior.
- (void)sharedPasswordController:(SharedPasswordController*)controller
             didAcceptSuggestion:(FormSuggestion*)suggestion;

@end

// Per-tab shared password controller. Handles parsing forms, loading
// suggestions, filling forms, and generating passwords.
@interface SharedPasswordController
    : NSObject <CRWWebStateObserver,
                PasswordManagerDriverBridge,
                PasswordSuggestionHelperDelegate,
                PasswordFormHelperDelegate,
                FormSuggestionProvider,
                FormActivityObserver>

// Helper contains common password form processing logic.
@property(nonatomic, readonly) PasswordFormHelper* formHelper;

// Delegate to receive callbacks from this class.
@property(nonatomic, weak) id<SharedPasswordControllerDelegate> delegate;

- (instancetype)initWithWebState:(web::WebState*)webState
                         manager:(password_manager::PasswordManagerInterface*)
                                     passwordManager
                      formHelper:(PasswordFormHelper*)formHelper
                suggestionHelper:(PasswordSuggestionHelper*)suggestionHelper
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_SHARED_PASSWORD_CONTROLLER_H_
