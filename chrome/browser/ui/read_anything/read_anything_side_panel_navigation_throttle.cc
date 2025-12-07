// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_side_panel_navigation_throttle.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/read_anything/read_anything_entry_point_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/base/page_transition_types.h"

// static
void ReadAnythingSidePanelNavigationThrottle::CreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  registry.AddThrottle(
      base::WrapUnique(new ReadAnythingSidePanelNavigationThrottle(registry)));
}

ReadAnythingSidePanelNavigationThrottle::ThrottleCheckResult
ReadAnythingSidePanelNavigationThrottle::WillStartRequest() {
  return HandleSidePanelRequest();
}

const char* ReadAnythingSidePanelNavigationThrottle::GetNameForLogging() {
  return "ReadAnythingSidePanelNavigationThrottle";
}

ReadAnythingSidePanelNavigationThrottle::
    ReadAnythingSidePanelNavigationThrottle(
        content::NavigationThrottleRegistry& registry)
    : NavigationThrottle(registry) {}

ReadAnythingSidePanelNavigationThrottle::ThrottleCheckResult
ReadAnythingSidePanelNavigationThrottle::HandleSidePanelRequest() {
  const auto& url = navigation_handle()->GetURL();
  // Only cancel the request if a user initiated it. Otherwise when reading mode
  // opens, it will hit this block and cause a stack overflow.
  if (url.GetWithEmptyPath() !=
          chrome::kChromeUIUntrustedReadAnythingSidePanelURL ||
      !ui::PageTransitionCoreTypeIs(navigation_handle()->GetPageTransition(),
                                    ui::PAGE_TRANSITION_TYPED)) {
    return content::NavigationThrottle::PROCEED;
  }
  Browser* browser =
      chrome::FindBrowserWithTab(navigation_handle()->GetWebContents());
  CHECK(browser);
  read_anything::ReadAnythingEntryPointController::ShowUI(
      browser, ReadAnythingOpenTrigger::kReadAnythingNavigationThrottle);
  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}
