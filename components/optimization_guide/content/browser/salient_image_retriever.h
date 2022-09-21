// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_SALIENT_IMAGE_RETRIEVER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_SALIENT_IMAGE_RETRIEVER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/opengraph/metadata.mojom-forward.h"

class OptimizationGuideLogger;
class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace optimization_guide {

// Provides callers with the salient image for a page.
// TODO(crbug.com/1349917): Consider moving this to //chrome/browser.
class SalientImageRetriever {
 public:
  explicit SalientImageRetriever(
      OptimizationGuideLogger* optimization_guide_logger);
  ~SalientImageRetriever();

  SalientImageRetriever(const SalientImageRetriever&) = delete;
  SalientImageRetriever& operator=(const SalientImageRetriever&) = delete;

  void GetOgImage(content::WebContents* web_contents);

 private:
  void OnGetOpenGraphMetadata(const GURL& page_url,
                              ukm::SourceId ukm_source_id,
                              blink::mojom::OpenGraphMetadataPtr metadata);

  // The logger that plumbs the debug logs to the optimization guide
  // internals page. Not owned. Guaranteed to outlive |this|, since the logger
  // and |this| are owned by the optimization guide keyed service.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  base::WeakPtrFactory<SalientImageRetriever> weak_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_SALIENT_IMAGE_RETRIEVER_H_
