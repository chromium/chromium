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
#include "components/optimization_guide/content/browser/page_context_eligibility_api.h"

namespace optimization_guide {

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

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_H_
