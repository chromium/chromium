// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_ACCESSIBILITY_ANDROID_PAGE_ZOOM_METRICS_H_
#define COMPONENTS_BROWSER_UI_ACCESSIBILITY_ANDROID_PAGE_ZOOM_METRICS_H_

#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

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
  void LogZoomLevelUKM(content::WebContents* web_contents,
                       double new_zoom_level);

  // Helper function for UKM logging
  static void LogZoomLevelUKMHelper(ukm::SourceId ukm_source_id,
                                    double new_zoom_level,
                                    ukm::UkmRecorder* ukm_recorder);
};

}  // namespace browser_ui

#endif  // COMPONENTS_BROWSER_UI_ACCESSIBILITY_ANDROID_PAGE_ZOOM_METRICS_H_
