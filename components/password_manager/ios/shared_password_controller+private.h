// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_SHARED_PASSWORD_CONTROLLER_PRIVATE_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_SHARED_PASSWORD_CONTROLLER_PRIVATE_H_

#import "components/password_manager/ios/shared_password_controller.h"

using autofill::FormData;
using autofill::FormRendererId;

// This is a private extension that is intended to expose otherwise private
// members for testing.
@interface SharedPasswordController ()

// Tracks if current password is generated.
@property(nonatomic, assign) BOOL isPasswordGenerated;

- (void)injectGeneratedPasswordForFormId:(FormRendererId)formIdentifier
                                 inFrame:(web::WebFrame*)frame
                       generatedPassword:(NSString*)generatedPassword
                       completionHandler:(void (^)())completionHandler;

- (void)didFinishPasswordFormExtraction:(const std::vector<FormData>&)forms
                  triggeredByFormChange:(BOOL)triggeredByFormChange
                                inFrame:(web::WebFrame*)frame;

@end

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_SHARED_PASSWORD_CONTROLLER_PRIVATE_H_
