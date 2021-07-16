// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/presentation/presentation_navigation_policy.h"

#include "content/public/browser/navigation_handle.h"

namespace media_router {

NavigationPolicy::~NavigationPolicy() = default;

DefaultNavigationPolicy::DefaultNavigationPolicy() = default;
DefaultNavigationPolicy::~DefaultNavigationPolicy() = default;

bool DefaultNavigationPolicy::AllowNavigation(content::NavigationHandle*) {
  return true;
}

PresentationNavigationPolicy::PresentationNavigationPolicy() = default;
PresentationNavigationPolicy::~PresentationNavigationPolicy() = default;

bool PresentationNavigationPolicy::AllowNavigation(
    content::NavigationHandle* navigation_handle) {
  // We only care about top-level navigations that are cross-document.
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return true;
  }

  // The initial navigation had already begun.
  if (first_navigation_started_) {
    return false;
  }

  first_navigation_started_ = true;
  return true;
}

}  // namespace media_router
