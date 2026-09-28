// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_ACTOR_LOGIN_ACTOR_LOGIN_TOOL_DELEGATE_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_ACTOR_LOGIN_ACTOR_LOGIN_TOOL_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

namespace web {
class WebState;
}  // namespace web

// Protocol implemented by password controllers to support Actor login form
// extraction.
@protocol ActorLoginToolDelegate <NSObject>

// Rescans `webState` for password forms and sends any found forms to the
// password manager.
//
// Forms that parse successfully are surfaced asynchronously via
// `PasswordFormCache` observers, not through this delegate.
//
// `noFormsFoundHandler` is invoked only when every same-origin frame finished
// extraction and none contained a password form, which lets callers fail fast
// instead of waiting on their own timeout. It is deliberately NOT invoked when
// a form is found, so it must not be used as a completion signal.
- (void)actorLoginToolRescansFormsInWebState:(web::WebState*)webState
                         noFormsFoundHandler:
                             (ProceduralBlock)noFormsFoundHandler;

@end

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_ACTOR_LOGIN_ACTOR_LOGIN_TOOL_DELEGATE_H_
