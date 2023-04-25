// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_SHARED_PASSWORD_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_SHARED_PASSWORD_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/form_util/form_activity_observer.h"
#include "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/ios/password_account_storage_notice_handler.h"
#import "components/password_manager/ios/password_controller_driver_helper.h"
#import "components/password_manager/ios/password_form_helper.h"
#import "components/password_manager/ios/password_generation_provider.h"
#import "components/password_manager/ios/password_manager_driver_bridge.h"
#import "components/password_manager/ios/password_suggestion_helper.h"
#import "ios/web/public/js_messaging/web_frames_manager_observer_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"

namespace password_manager {
class PasswordManagerClient;
}  // namespace password_manager

@class FormSuggestion;
@class SharedPasswordController;

// Protocol to define methods that must be implemented by the embedder.
@protocol
    SharedPasswordControllerDelegate <NSObject,
                                      PasswordsAccountStorageNoticeHandler>

// The PasswordManagerClient owned by the delegate.
@property(nonatomic, readonly)
    password_manager::PasswordManagerClient* passwordManagerClient;

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

// Adds event listeners to fields which are associated with a bottom sheet.
// When the focus event occurs on these fields, a bottom sheet will be shown
// instead of the keyboard, allowing the user to fill the fields by tapping
// one of the suggestions.
- (void)attachListenersForBottomSheet:
            (const std::vector<autofill::FieldRendererId>&)rendererIds
                              inFrame:(web::WebFrame*)frame;

@end

// Per-tab shared password controller. Handles parsing forms, loading
// suggestions, filling forms, and generating passwords.
@interface SharedPasswordController
    : NSObject <CRWWebFramesManagerObserver,
                CRWWebStateObserver,
                FormActivityObserver,
                FormSuggestionProvider,
                PasswordFormHelperDelegate,
                PasswordGenerationProvider,
                PasswordManagerDriverBridge,
                PasswordSuggestionHelperDelegate>

// Helper contains common password form processing logic.
@property(nonatomic, readonly) PasswordFormHelper* formHelper;

// Delegate to receive callbacks from this class.
@property(nonatomic, weak) id<SharedPasswordControllerDelegate> delegate;

- (instancetype)initWithWebState:(web::WebState*)webState
                         manager:(password_manager::PasswordManagerInterface*)
                                     passwordManager
                      formHelper:(PasswordFormHelper*)formHelper
                suggestionHelper:(PasswordSuggestionHelper*)suggestionHelper
                    driverHelper:(PasswordControllerDriverHelper*)driverHelper
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_SHARED_PASSWORD_CONTROLLER_H_
