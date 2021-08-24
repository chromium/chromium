// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_NAVIGATION_THROTTLE_H_

#include <memory>
#include <string>

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

class GURL;
class Profile;

namespace ash {

// Routes navigation to the Projector app if the user clicks a Projector link.
class ProjectorNavigationThrottle : public content::NavigationThrottle {
 public:
  // Returns a new throttle for the given navigation if the |kProjector| feature
  // flag is enabled, or nullptr otherwise.
  static std::unique_ptr<ProjectorNavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* navigation_handle);

  ProjectorNavigationThrottle(const ProjectorNavigationThrottle&) = delete;
  ProjectorNavigationThrottle& operator=(const ProjectorNavigationThrottle&) =
      delete;
  ~ProjectorNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;

 private:
  explicit ProjectorNavigationThrottle(
      content::NavigationHandle* navigation_handle);

  void LaunchProjectorApp(const GURL& override_url);

  GURL ConstructOverrideUrl(const GURL& url);

  Profile* const profile_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_NAVIGATION_THROTTLE_H_
