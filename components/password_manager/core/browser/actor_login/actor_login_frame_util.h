// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_FRAME_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_FRAME_UTIL_H_

namespace password_manager {
class PasswordManagerDriver;
}  // namespace password_manager

namespace url {
class Origin;
}  // namespace url

namespace actor_login {

// Returns whether `form_origin` is supported relative to `main_frame_origin`
// (requires same domain or host).
bool IsFormOriginSupported(const url::Origin& form_origin,
                           const url::Origin& main_frame_origin);

// Returns whether a frame is considered safe and eligible to fill based on its
// origin, main frame origin, and frame hierarchy properties.
//
// A frame is eligible if:
// 1. It is not nested within a fenced frame.
// 2. Its origin is supported relative to `main_frame_origin` (same
// domain/host).
// 3. It is either the primary main frame, a direct child of the primary main
//    frame, or a nested frame with no cross-origin ancestors.
bool IsValidFrameAndOriginToFill(const url::Origin& frame_origin,
                                 const url::Origin& main_frame_origin,
                                 bool is_nested_within_fenced_frame,
                                 bool is_in_primary_main_frame,
                                 bool is_direct_child,
                                 bool has_cross_origin_ancestor);

// Checks whether `driver` represents a frame context that is valid and eligible
// to fill for `main_frame_origin`.
bool IsValidFrameAndOriginToFill(
    password_manager::PasswordManagerDriver* driver,
    const url::Origin& main_frame_origin);

}  // namespace actor_login

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ACTOR_LOGIN_ACTOR_LOGIN_FRAME_UTIL_H_
