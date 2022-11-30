// Copyright 2017 The Chromium Authors
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
