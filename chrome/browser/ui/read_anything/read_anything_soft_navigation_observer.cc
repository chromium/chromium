// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_soft_navigation_observer.h"

#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/accessibility_features.h"

ReadAnythingSoftNavigationObserver::ReadAnythingSoftNavigationObserver() =
    default;

ReadAnythingSoftNavigationObserver::~ReadAnythingSoftNavigationObserver() =
    default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ReadAnythingSoftNavigationObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ReadAnythingSoftNavigationObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Return CONTINUE_OBSERVING so that we remain attached to the
  // PageLoadTracker. If we return STOP_OBSERVING, this observer will be
  // destroyed and we will miss any soft navigations that occur after the
  // prerendered page activates and becomes the primary page.
  return CONTINUE_OBSERVING;
}

void ReadAnythingSoftNavigationObserver::OnSoftNavigation() {
  if (!features::IsImmersiveReadAnythingEnabled()) {
    return;
  }

  content::WebContents* web_contents = GetDelegate().GetWebContents();
  if (!web_contents) {
    return;
  }
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
  if (!tab) {
    return;
  }
  ReadAnythingController* controller = ReadAnythingController::From(tab);
  if (controller) {
    controller->OnSoftNavigation();
  }
}
