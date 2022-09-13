// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/salient_image_retriever.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/opengraph/metadata.mojom.h"
#include "url/gurl.h"

namespace optimization_guide {

SalientImageRetriever::SalientImageRetriever(
    OptimizationGuideLogger* optimization_guide_logger)
    : optimization_guide_logger_(optimization_guide_logger) {}
SalientImageRetriever::~SalientImageRetriever() = default;

void SalientImageRetriever::GetOgImage(content::WebContents* web_contents) {
  content::RenderFrameHost& main_frame =
      web_contents->GetPrimaryPage().GetMainDocument();

  main_frame.GetOpenGraphMetadata(base::BindOnce(
      &SalientImageRetriever::OnGetOpenGraphMetadata,
      weak_factory_.GetWeakPtr(), main_frame.GetLastCommittedURL()));
}

void SalientImageRetriever::OnGetOpenGraphMetadata(
    const GURL& page_url,
    blink::mojom::OpenGraphMetadataPtr metadata) {
  const char og_image_availability_histogram_name[] =
      "OptimizationGuide.PageContentAnnotations.SalientImageAvailability";
  // Keep in sync with OptimizationGuideSalientImageAvailability histogram enum.
  enum class SalientImageAvailability {
    kUnknown = 0,
    kNotAvailable = 1,
    kAvailableButUnparsableFromOgImage = 2,
    kAvailableFromOgImage = 3,
    kMaxValue = kAvailableFromOgImage
  };

  if (!metadata || !metadata->image) {
    base::UmaHistogramEnumeration(og_image_availability_histogram_name,
                                  SalientImageAvailability::kNotAvailable);
    return;
  }

  GURL url(metadata->image.value());
  if (url.is_empty() || !url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    base::UmaHistogramEnumeration(
        og_image_availability_histogram_name,
        SalientImageAvailability::kAvailableButUnparsableFromOgImage);
    return;
  }

  base::UmaHistogramEnumeration(
      og_image_availability_histogram_name,
      SalientImageAvailability::kAvailableFromOgImage);

  OPTIMIZATION_GUIDE_LOGGER(
      optimization_guide_common::mojom::LogSource::PAGE_CONTENT_ANNOTATIONS,
      optimization_guide_logger_)
      << " page_url=" << page_url
      << " Salient Image URL: " << metadata->image.value();
}

}  // namespace optimization_guide