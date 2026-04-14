// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_context_eligibility.h"

#include <memory>
#include <span>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_api.h"
#include "components/optimization_guide/core/optimization_guide_library_holder.h"

namespace optimization_guide {

BASE_FEATURE(kUseCppPageContextEligibilityInterface,
             base::FEATURE_ENABLED_BY_DEFAULT);

// A temporary wrapper to bridge the legacy C ABI interface to the new C++
// interface.
// TODO(crbug.com/421932889): Remove this once all callers migrate to the C++
// interface.
class PageContextEligibilityApiWrapper : public PageContextEligibilityApi {
 public:
  explicit PageContextEligibilityApiWrapper(
      const PageContextEligibilityAPI& api)
      : api_(api) {}
  ~PageContextEligibilityApiWrapper() override = default;

  bool IsPageContextEligible(
      const std::string& host,
      const std::string& path,
      const std::vector<FrameMetadata>& frame_metadata) const override {
    return api_->IsPageContextEligible(host, path, frame_metadata);
  }

  bool IsPageContextEligibleWithAccount(
      const std::string& host,
      const std::string& path,
      const std::string& account,
      const std::vector<FrameMetadata>& frame_metadata) const override {
    return api_->IsPageContextEligibleWithAccount(host, path, account,
                                                  frame_metadata);
  }

  bool ShouldReextractPageContext(
      const std::string& host,
      const std::string& path,
      const std::vector<std::string>& updated_meta_tags) const override {
    return api_->ShouldReextractPageContext(host, path, updated_meta_tags);
  }

  std::span<const std::string_view> GetMetaTagNamesAffectingEligibility(
      const std::string& host,
      const std::string& path,
      const std::vector<FrameMetadata>& frame_metadata) const override {
    StringViewSpan span =
        api_->GetMetaTagNamesAffectingEligibility(host, path, frame_metadata);
    return std::span<const std::string_view>(span.data, span.size);
  }

  raw_ref<const PageContextEligibilityAPI> api_;
};

PageContextEligibility::PageContextEligibility(
    const PageContextEligibilityApi* api)
    : api_(*api) {}

PageContextEligibility::PageContextEligibility(
    const PageContextEligibilityAPI* api)
    : owned_api_(std::make_unique<PageContextEligibilityApiWrapper>(*api)),
      api_(*owned_api_) {}

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

  // Try the new C++ interface first.
  if (base::FeatureList::IsEnabled(kUseCppPageContextEligibilityInterface)) {
    auto get_api_new = reinterpret_cast<PageContextEligibilityApiGetter>(
        holder_ptr->GetFunctionPointer("GetPageContextEligibilityApi"));
    if (get_api_new) {
      const PageContextEligibilityApi* api = get_api_new();
      if (api) {
        return base::WrapUnique(new PageContextEligibility(api));
      }
    }
  }

  // Fallback to the old C interface.
  auto get_api_old = reinterpret_cast<PageContextEligibilityAPIGetter>(
      holder_ptr->GetFunctionPointer("GetPageContextEligibilityAPI"));
  if (get_api_old) {
    const PageContextEligibilityAPI* api_old = get_api_old();
    if (api_old) {
      return base::WrapUnique(new PageContextEligibility(api_old));
    }
  }

  return {};
}

std::vector<FrameMetadata> GetFrameMetadataFromPageContent(
    const AIPageContentResult& result) {
  std::vector<FrameMetadata> frame_metadata_structs;
  const auto& page_metadata = result.metadata;
  frame_metadata_structs.reserve(page_metadata->frame_metadata.size());
  for (auto& frame_metadata_mojom : page_metadata->frame_metadata) {
    std::vector<MetaTag> meta_tags;
    meta_tags.reserve(frame_metadata_mojom->meta_tags.size());
    for (auto& tag : frame_metadata_mojom->meta_tags) {
      MetaTag meta_tag(tag->name, tag->content);
      meta_tags.push_back(std::move(meta_tag));
    }
    FrameMetadata metadata(frame_metadata_mojom->url.GetHost(),
                           frame_metadata_mojom->url.GetPath(),
                           std::move(meta_tags));
    frame_metadata_structs.push_back(std::move(metadata));
  }
  return frame_metadata_structs;
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
