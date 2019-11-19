// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/web_footer_experiment_view.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/common/webui_url_constants.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"

WebFooterExperimentView::WebFooterExperimentView(Profile* profile)
    : WebView(profile), metrics_collector_(GetWebContents()) {
  startup_metric_utils::RecordWebFooterCreation(base::TimeTicks::Now());
  LoadInitialURL(GURL(chrome::kChromeUIWebFooterExperimentURL));
  task_manager::WebContentsTags::CreateForTabContents(web_contents());
}

WebFooterExperimentView::FirstPaintMetricsCollector::FirstPaintMetricsCollector(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

WebFooterExperimentView::FirstPaintMetricsCollector::
    ~FirstPaintMetricsCollector() = default;

void WebFooterExperimentView::FirstPaintMetricsCollector::
    DidFirstVisuallyNonEmptyPaint() {
  startup_metric_utils::RecordWebFooterDidFirstVisuallyNonEmptyPaint(
      base::TimeTicks::Now());
}
