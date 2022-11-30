// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_PRESENTATION_NAVIGATION_POLICY_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_PRESENTATION_NAVIGATION_POLICY_H_

namespace content {
class NavigationHandle;
}  // namespace content

namespace media_router {

// Selected calls to the navigation methods in WebContentsObserver are
// delegated to this object to determine whether a navigation is allowed.  If
// any call returns false, the presentation tab is destroyed.
class NavigationPolicy {
 public:
  virtual ~NavigationPolicy();
  virtual bool AllowNavigation(
      content::NavigationHandle* navigation_handle) = 0;
};

// This default policy allows all navigations.
class DefaultNavigationPolicy : public NavigationPolicy {
 public:
  DefaultNavigationPolicy();
  ~DefaultNavigationPolicy() override;

  bool AllowNavigation(content::NavigationHandle* navigation_handle) override;
};

// Navigation policy for presentations, where top-level navigations are not
// allowed.
class PresentationNavigationPolicy : public NavigationPolicy {
 public:
  PresentationNavigationPolicy();
  ~PresentationNavigationPolicy() override;

  bool AllowNavigation(content::NavigationHandle* navigation_handle) override;

 private:
  bool first_navigation_started_ = false;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_PRESENTATION_PRESENTATION_NAVIGATION_POLICY_H_
