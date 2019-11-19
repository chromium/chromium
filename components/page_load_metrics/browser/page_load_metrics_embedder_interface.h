// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_EMBEDDER_INTERFACE_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_EMBEDDER_INTERFACE_H_

#include <memory>

class GURL;

namespace base {
class OneShotTimer;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace page_load_metrics {

class PageLoadTracker;

// This class serves as a functional interface to various chrome// features.
// Impl version is defined in components/page_load_metrics/browser.
class PageLoadMetricsEmbedderInterface {
 public:
  virtual ~PageLoadMetricsEmbedderInterface() {}
  virtual bool IsNewTabPageUrl(const GURL& url) = 0;
  virtual void RegisterObservers(PageLoadTracker* metrics) = 0;
  virtual std::unique_ptr<base::OneShotTimer> CreateTimer() = 0;
  virtual bool IsPrerender(content::WebContents* web_contents) = 0;
  virtual bool IsExtensionUrl(const GURL& url) = 0;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_EMBEDDER_INTERFACE_H_
