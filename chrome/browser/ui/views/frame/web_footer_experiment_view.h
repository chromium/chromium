// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WEB_FOOTER_EXPERIMENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WEB_FOOTER_EXPERIMENT_VIEW_H_

#include "content/public/browser/web_contents_observer.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/metadata/metadata_header_macros.h"

class Profile;

class WebFooterExperimentView : public views::WebView {
 public:
  METADATA_HEADER(WebFooterExperimentView);
  explicit WebFooterExperimentView(Profile* profile);
  WebFooterExperimentView(const WebFooterExperimentView&) = delete;
  WebFooterExperimentView& operator=(const WebFooterExperimentView&) = delete;

 private:
  class FirstPaintMetricsCollector : public content::WebContentsObserver {
   public:
    explicit FirstPaintMetricsCollector(content::WebContents* web_contents);
    FirstPaintMetricsCollector(const FirstPaintMetricsCollector&) = delete;
    FirstPaintMetricsCollector& operator=(const FirstPaintMetricsCollector&) =
        delete;
    ~FirstPaintMetricsCollector() override;

   private:
    // content::WebContentsObserver:
    void DidFirstVisuallyNonEmptyPaint() override;
  };

  FirstPaintMetricsCollector metrics_collector_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WEB_FOOTER_EXPERIMENT_VIEW_H_
