// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_native_library.h"
#include "base/types/pass_key.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_api.h"

namespace optimization_guide {

BASE_DECLARE_FEATURE(kUseCppPageContextEligibilityInterface);

class PageContextEligibilityApiWrapper;

// TODO(crbug.com/421932889): Forward declare PageContextEligibility so that
// callers cannot invoke api() directly.
class PageContextEligibility {
 public:
  // Constructor for the new C++ interface (Option 1).
  // The API pointer is owned by the library (shared library).
  explicit PageContextEligibility(const PageContextEligibilityApi* api);

  // Constructor for the legacy C ABI interface.
  // The wrapper is created on the heap and owned by this class.
  // TODO(crbug.com/421932889): Remove this once all callers migrate to the C++
  // interface.
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

  // Exposes the raw PageContextEligibilityApi functions defined by the library.
  // TODO(b/490161242): Expose the functions on this class instead of exposing
  // the raw API.
  const PageContextEligibilityApi& api() const { return *api_; }

 private:
  static std::unique_ptr<PageContextEligibility> Create();

  // Only populated if we are using the backward compatibility wrapper.
  std::unique_ptr<const PageContextEligibilityApiWrapper> owned_api_;
  raw_ref<const PageContextEligibilityApi> api_;
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
