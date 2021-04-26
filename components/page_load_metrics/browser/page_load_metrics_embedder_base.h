// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_EMBEDDER_BASE_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_EMBEDDER_BASE_H_

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

  // PageLoadMetricsEmbedderInterface.
  void RegisterObservers(PageLoadTracker* metrics) final;
  std::unique_ptr<base::OneShotTimer> CreateTimer() override;

  content::WebContents* web_contents() const { return web_contents_; }

 protected:
  ~PageLoadMetricsEmbedderBase() override;
  virtual void RegisterEmbedderObservers(PageLoadTracker* tracker) = 0;
  virtual bool IsPrerendering() const = 0;

 private:
  content::WebContents* const web_contents_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_EMBEDDER_BASE_H_
