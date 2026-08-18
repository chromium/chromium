// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}  // namespace content

// Intercepts link clicks to the web-based Skills settings URL from the Skills
// UI or WebUI and redirects them to `chrome://settings/ai/skills`.
class SkillsNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit SkillsNavigationThrottle(
      content::NavigationThrottleRegistry& registry);
  ~SkillsNavigationThrottle() override;

  SkillsNavigationThrottle(const SkillsNavigationThrottle&) = delete;
  SkillsNavigationThrottle& operator=(const SkillsNavigationThrottle&) = delete;

  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SKILLS_SKILLS_NAVIGATION_THROTTLE_H_
