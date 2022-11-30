// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_CONTROLLER_DRIVER_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_CONTROLLER_DRIVER_HELPER_H_

#import <Foundation/Foundation.h>

#import "components/password_manager/core/browser/password_generation_frame_helper.h"
#import "components/password_manager/ios/ios_password_manager_driver.h"

NS_ASSUME_NONNULL_BEGIN

@class PasswordControllerDriverHelper;

namespace web {
class WebFrame;
class WebState;
}  // namespace web

// This class is a helper for SharedPasswordController which is able to
// retrieve the driver and the generation helper. These methods are separated
// inside this class in order to facilitate tests which verify flows involving
// the driver and the generation helper.
@interface PasswordControllerDriverHelper : NSObject

// Creates a instance with the given |webState|.
- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Returns the IOSPasswordManagerDriver tied to the frame.
- (IOSPasswordManagerDriver*)PasswordManagerDriver:(web::WebFrame*)frame;

// Returns the PasswordGenerationFrameHelper correspondent to the
// IOSPasswordManagerDriver attached to the frame.
- (password_manager::PasswordGenerationFrameHelper*)PasswordGenerationHelper:
    (web::WebFrame*)frame;

@end

NS_ASSUME_NONNULL_END

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_CONTROLLER_DRIVER_HELPER_H_
