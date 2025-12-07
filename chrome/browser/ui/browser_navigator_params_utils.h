// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_NAVIGATOR_PARAMS_UTILS_H_
#define CHROME_BROWSER_UI_BROWSER_NAVIGATOR_PARAMS_UTILS_H_

#include "content/public/browser/navigation_controller.h"

namespace content {
class WebContents;
}  // namespace content

struct NavigateParams;

// Creates a content::NavigationController::LoadURLParams object from a
// NavigateParams object.
content::NavigationController::LoadURLParams LoadURLParamsFromNavigateParams(
    NavigateParams* params);

#if !BUILDFLAG(IS_ANDROID)
// Same as previous but sets navigation UI data for main frame navigations.
content::NavigationController::LoadURLParams LoadURLParamsFromNavigateParams(
    content::WebContents* target_contents,
    NavigateParams* params);
#endif

#endif  // CHROME_BROWSER_UI_BROWSER_NAVIGATOR_PARAMS_UTILS_H_
