// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_ACTOR_LOGIN_ACTOR_LOGIN_TOOL_DELEGATE_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_ACTOR_LOGIN_ACTOR_LOGIN_TOOL_DELEGATE_H_

#import <Foundation/Foundation.h>

namespace web {
class WebState;
}  // namespace web

// Protocol implemented by password controllers to support Actor login form
// extraction.
@protocol ActorLoginToolDelegate <NSObject>

// Finds password forms in `webState` and sends them to the password manager.
// Calls `completionHandler` with YES if any password forms were found, and NO
// otherwise.
- (void)actorLoginToolFindsFormsInWebState:(web::WebState*)webState
                         completionHandler:
                             (void (^)(BOOL found))completionHandler;

@end

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_ACTOR_LOGIN_ACTOR_LOGIN_TOOL_DELEGATE_H_
