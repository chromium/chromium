// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_context_eligibility.h"

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_api.h"
#include "components/optimization_guide/core/optimization_guide_library_holder.h"

namespace optimization_guide {

PageContextEligibility::PageContextEligibility(
    const PageContextEligibilityAPI* api)
    : api_(api) {}
PageContextEligibility::~PageContextEligibility() = default;

// static
DISABLE_CFI_DLSYM
PageContextEligibility* PageContextEligibility::Get() {
  static base::NoDestructor<std::unique_ptr<PageContextEligibility>>
      page_context_eligibility{Create()};
  return page_context_eligibility->get();
}

// static
DISABLE_CFI_DLSYM
std::unique_ptr<PageContextEligibility> PageContextEligibility::Create() {
  // TODO(crbug.com/414828945): Move this creation out of this file if multiple
  // use cases for it in browser.
  static base::NoDestructor<std::unique_ptr<OptimizationGuideLibraryHolder>>
      holder{OptimizationGuideLibraryHolder::Create()};
  // Pointer will be null if the library was not created.
  OptimizationGuideLibraryHolder* holder_ptr = holder->get();
  if (!holder_ptr) {
    return {};
  }

  auto get_api = reinterpret_cast<PageContextEligibilityAPIGetter>(
      holder_ptr->GetFunctionPointer("GetPageContextEligibilityAPI"));
  if (!get_api) {
    return {};
  }

  const PageContextEligibilityAPI* api = get_api();
  if (!api) {
    return {};
  }
  return base::WrapUnique(new PageContextEligibility(api));
}

DISABLE_CFI_DLSYM
bool IsPageContextEligible(
    const std::string& host,
    const std::string& path,
    const std::vector<optimization_guide::FrameMetadata>& frame_metadata,
    const PageContextEligibility* api_holder) {
  // TODO(crbug.com/421932889): `api_holder` should not be provided by caller
  // and instead be retrieved as part of this function call.
  if (!api_holder) {
    return true;
  }

  return api_holder->api().IsPageContextEligible(host, path,
                                                 std::move(frame_metadata));
}

}  // namespace optimization_guide
