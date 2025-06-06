// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_native_library.h"
#include "base/types/pass_key.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_api.h"

namespace optimization_guide {

// TODO(crbug.com/421932889): Forward declare PageContextEligibility so that
// callers cannot invoke api() directly.
class PageContextEligibility {
 public:
  explicit PageContextEligibility(const PageContextEligibilityAPI* api);
  ~PageContextEligibility();
  PageContextEligibility(const PageContextEligibility& other) = delete;
  PageContextEligibility& operator=(const PageContextEligibility& other) =
      delete;
  PageContextEligibility(PageContextEligibility&& other) = delete;
  PageContextEligibility& operator=(PageContextEligibility&& other) = delete;

  // Gets a lazily initialized global instance of PageContextEligibility. May
  // return null if the underlying library could not be loaded.
  static PageContextEligibility* Get();

  // Exposes the raw PageContextEligibilityAPI functions defined by the library.
  const PageContextEligibilityAPI& api() const { return *api_; }

 private:
  static std::unique_ptr<PageContextEligibility> Create();

  raw_ptr<const PageContextEligibilityAPI> api_;
};

// Convert the page metadata from the `result` to a vector of `FrameMetadata`.
std::vector<optimization_guide::FrameMetadata> GetFrameMetadataFromPageContent(
    const optimization_guide::AIPageContentResult& result);

// Checks if the page is context eligible using the api provided in
// `api_holder`. This function must be called instead of the function in the API
// directly in order to have properly disabled CFI.
bool IsPageContextEligible(
    const std::string& host,
    const std::string& path,
    const std::vector<optimization_guide::FrameMetadata>& frame_metadata,
    const PageContextEligibility* api_holder);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_H_
