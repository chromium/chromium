// Copyright 2016 The Chromium Authors
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
class BrowserContext;
class NavigationHandle;
class WebContents;
}  // namespace content

namespace page_load_metrics {

class PageLoadMetricsMemoryTracker;
class PageLoadTracker;

// This class serves as a functional interface to various chrome// features.
// Impl version is defined in components/page_load_metrics/browser.
class PageLoadMetricsEmbedderInterface {
 public:
  virtual ~PageLoadMetricsEmbedderInterface() = default;
  virtual bool IsNewTabPageUrl(const GURL& url) = 0;
  virtual void RegisterObservers(
      PageLoadTracker* metrics,
      content::NavigationHandle* navigation_handle) = 0;
  virtual std::unique_ptr<base::OneShotTimer> CreateTimer() = 0;
  virtual bool IsNoStatePrefetch(content::WebContents* web_contents) = 0;
  virtual bool IsExtensionUrl(const GURL& url) = 0;
  virtual bool IsSidePanel(content::WebContents* web_contents) = 0;
  virtual bool IsNonTabWebUI() = 0;

  // Returns the PageLoadMetricsMemoryTracker for the given BrowserContext if
  // tracking is enabled.
  virtual PageLoadMetricsMemoryTracker* GetMemoryTrackerForBrowserContext(
      content::BrowserContext* browser_context) = 0;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_EMBEDDER_INTERFACE_H_
