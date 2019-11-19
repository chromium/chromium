// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WEB_FOOTER_EXPERIMENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WEB_FOOTER_EXPERIMENT_VIEW_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/controls/webview/webview.h"

class Profile;

class WebFooterExperimentView : public views::WebView {
 public:
  explicit WebFooterExperimentView(Profile* profile);

 private:
  class FirstPaintMetricsCollector : public content::WebContentsObserver {
   public:
    explicit FirstPaintMetricsCollector(content::WebContents* web_contents);
    ~FirstPaintMetricsCollector() override;

   private:
    // content::WebContentsObserver:
    void DidFirstVisuallyNonEmptyPaint() override;

    DISALLOW_COPY_AND_ASSIGN(FirstPaintMetricsCollector);
  };

  FirstPaintMetricsCollector metrics_collector_;

  DISALLOW_COPY_AND_ASSIGN(WebFooterExperimentView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WEB_FOOTER_EXPERIMENT_VIEW_H_
