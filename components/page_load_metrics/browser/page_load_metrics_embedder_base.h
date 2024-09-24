// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_EMBEDDER_BASE_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_EMBEDDER_BASE_H_

#include "base/memory/raw_ptr.h"
#include "components/page_load_metrics/browser/page_load_metrics_embedder_interface.h"

namespace page_load_metrics {

class PageLoadTracker;

// This is base class for PageLoadMetricsEmbedderInterface implementation, it
// registers components' observers which are common among the embedders, while,
// the embedder implementation may override RegisterEmbedderObservers() for its
// specific observers.
class PageLoadMetricsEmbedderBase : public PageLoadMetricsEmbedderInterface {
 public:
  explicit PageLoadMetricsEmbedderBase(content::WebContents* web_contents);

  // PageLoadMetricsEmbedderInterface:
  void RegisterObservers(PageLoadTracker* tracker,
                         content::NavigationHandle* navigation_handle) override;
  std::unique_ptr<base::OneShotTimer> CreateTimer() override;

  content::WebContents* web_contents() const { return web_contents_; }

 protected:
  // Registers the common page load metrics observers that should be added for
  // regular web pages. This should be called by subclasses in their
  // `RegisterObservers()` override after performing any page-specific checks
  // to determine whether common observers should be installed.
  //
  // This method should not be called for pages that don't represent regular
  // web page loads, such as:
  // - Special chrome:// pages (e.g., chrome://new-tab-page)
  // - Extension pages (chrome-extension://...)
  // - Side panels
  // - Non-tab Web UI pages
  //
  // For these cases, subclasses should implement their own page-specific
  // observer registration logic.
  void RegisterCommonObservers(PageLoadTracker* tracker);

  ~PageLoadMetricsEmbedderBase() override;

 private:
  const raw_ptr<content::WebContents> web_contents_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_EMBEDDER_BASE_H_
