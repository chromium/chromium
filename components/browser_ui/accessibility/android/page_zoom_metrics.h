// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_ACCESSIBILITY_ANDROID_PAGE_ZOOM_METRICS_H_
#define COMPONENTS_BROWSER_UI_ACCESSIBILITY_ANDROID_PAGE_ZOOM_METRICS_H_

namespace content {
class WebContents;
}

namespace browser_ui {

/*
 * Native component to log metrics for Page Zoom.
 */
class PageZoomMetrics {
 public:
  PageZoomMetrics();
  ~PageZoomMetrics();

  // Logs UKM with the current zoom level for the specified WebContents.
  void LogZoomLevelUKM(content::WebContents* web_contents, double newZoomLevel);
};

}  // namespace browser_ui

#endif  // COMPONENTS_BROWSER_UI_ACCESSIBILITY_ANDROID_PAGE_ZOOM_METRICS_H_
