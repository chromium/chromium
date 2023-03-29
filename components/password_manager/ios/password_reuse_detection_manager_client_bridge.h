// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_REUSE_DETECTION_MANAGER_CLIENT_BRIDGE_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_REUSE_DETECTION_MANAGER_CLIENT_BRIDGE_H_

#import <Foundation/Foundation.h>

class GURL;

namespace safe_browsing {
enum class WarningAction;
}  // namespace safe_browsing

namespace web {
class WebState;
}  // namespace web

// C++ to ObjC bridge for methods of PasswordReuseDetectionManagerClient.
@protocol PasswordReuseDetectionManagerClientBridge

@property(readonly, nonatomic) web::WebState* webState;

@property(readonly, nonatomic) const GURL& lastCommittedURL;

// Shows Password Protection warning with |warningText|. |completion| should be
// called when the UI is dismissed with the user's |action|.
- (void)showPasswordProtectionWarning:(NSString*)warningText
                           completion:(void (^)(safe_browsing::WarningAction))
                                          completion;

@end

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_PASSWORD_REUSE_DETECTION_MANAGER_CLIENT_BRIDGE_H_
