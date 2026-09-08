// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/actor_login_frame_util.h"

#include "build/build_config.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/origin.h"

namespace actor_login {

bool IsFormOriginSupported(const url::Origin& form_origin,
                           const url::Origin& main_frame_origin) {
  return net::registry_controlled_domains::SameDomainOrHost(
      form_origin, main_frame_origin,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool IsValidFrameAndOriginToFill(const url::Origin& frame_origin,
                                 const url::Origin& main_frame_origin,
                                 bool is_nested_within_fenced_frame,
                                 bool is_in_primary_main_frame,
                                 bool is_direct_child,
                                 bool has_cross_origin_ancestor) {
  if (is_nested_within_fenced_frame) {
    // Fenced frames should not be filled.
    return false;
  }

  if (!IsFormOriginSupported(frame_origin, main_frame_origin)) {
    return false;
  }

  // We can fill a form if its frame context is considered safe and not overly
  // nested. A "fillable context" is either the primary main frame itself,
  // a direct child of the primary main frame that is not a fenced frame, or
  // a nested frame that is same-origin with the main frame and has no
  // cross-origin ancestors.
  return !has_cross_origin_ancestor || is_in_primary_main_frame ||
         is_direct_child;
}

bool IsValidFrameAndOriginToFill(
    password_manager::PasswordManagerDriver* driver,
    const url::Origin& main_frame_origin) {
  if (!driver) {
    return false;
  }

  if (driver->IsNestedWithinFencedFrame()) {
    // Fenced frames should not be filled.
    return false;
  }

  const url::Origin& frame_origin = driver->GetLastCommittedOrigin();
  if (!IsFormOriginSupported(frame_origin, main_frame_origin)) {
    return false;
  }

  // TODO(crbug.com/539923959): The following is done to provide a close-enough
  // value for iOS. Remove the flag guard once iOS has implemented
  // `IOSPasswordManagerDriver::HasCrossOriginAncestor()`; this relies on the
  // ancestors of a web frame being trackable.
#if BUILDFLAG(IS_IOS)
  bool has_cross_origin_ancestor =
      !frame_origin.IsSameOriginWith(main_frame_origin);
#else
  bool has_cross_origin_ancestor = driver->HasCrossOriginAncestor();
#endif

  bool is_in_primary_main_frame = driver->IsInPrimaryMainFrame();

  // We can fill a form if its frame context is considered safe and not overly
  // nested. A "fillable context" is either the primary main frame itself,
  // a direct child of the primary main frame that is not a fenced frame, or
  // a nested frame that is same-origin with the main frame and has no
  // cross-origin ancestors.
  //
  // Short-circuiting behavior: only call `IsDirectChildOfPrimaryMainFrame()`
  // if the frame has a cross-origin ancestor and is not the primary main frame.
  // This avoids calling `IsDirectChildOfPrimaryMainFrame()` for primary main
  // frames (which on iOS hits NOTREACHED()).
  bool is_direct_child = false;
  if (has_cross_origin_ancestor && !is_in_primary_main_frame) {
    is_direct_child = driver->IsDirectChildOfPrimaryMainFrame();
  }

  return IsValidFrameAndOriginToFill(
      frame_origin, main_frame_origin, /*is_nested_within_fenced_frame=*/false,
      is_in_primary_main_frame, is_direct_child, has_cross_origin_ancestor);
}

}  // namespace actor_login
