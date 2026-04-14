// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_API_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_API_H_

#include <stddef.h>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace optimization_guide {

// A meta tag represented by its name and content attributes.
struct MetaTag {
  explicit MetaTag(const std::string& name, const std::string& content);
  MetaTag(const MetaTag& other);
  MetaTag& operator=(const MetaTag& other);
  ~MetaTag();

  std::string name;
  std::string content;
};

// Metadata about a frame.
struct FrameMetadata {
  explicit FrameMetadata(const std::string& host,
                         const std::string& path,
                         std::vector<MetaTag> meta_tags);
  FrameMetadata(const FrameMetadata& other);
  FrameMetadata& operator=(const FrameMetadata& other);
  ~FrameMetadata();

  // The host of the URL of the frame.
  std::string host;
  // The path of the URL of the frame.
  std::string path;
  std::vector<MetaTag> meta_tags;
};

// New C++ Interface. This allows us to pass C++ objects rather than requiring
// everything to be C ABI compatible. When adding a new method to the library,
// you can define a default implementation with `NOTIMPLEMENTED()` here first.
class PageContextEligibilityApi {
 public:
  virtual ~PageContextEligibilityApi() = default;

  virtual bool IsPageContextEligible(
      const std::string& host,
      const std::string& path,
      const std::vector<FrameMetadata>& frame_metadata) const = 0;

  virtual bool IsPageContextEligibleWithAccount(
      const std::string& host,
      const std::string& path,
      const std::string& account,
      const std::vector<FrameMetadata>& frame_metadata) const = 0;

  virtual bool ShouldReextractPageContext(
      const std::string& host,
      const std::string& path,
      const std::vector<std::string>& updated_meta_tags) const = 0;

  virtual std::span<const std::string_view> GetMetaTagNamesAffectingEligibility(
      const std::string& host,
      const std::string& path,
      const std::vector<FrameMetadata>& frame_metadata) const = 0;
};

using PageContextEligibilityApiGetter = const PageContextEligibilityApi* (*)();

}  // namespace optimization_guide

extern "C" {

namespace optimization_guide {

// Represents an span of string views. Used instead of a std::span to avoid ABI
// issues.
struct StringViewSpan {
  const std::string_view* data;
  size_t size;
};

// Legacy C ABI interface for PageContextEligibility.
struct PageContextEligibilityAPI {
  // Whether the page is context eligible.
  bool (*IsPageContextEligible)(
      const std::string& host,
      const std::string& path,
      const std::vector<optimization_guide::FrameMetadata>& frame_metadata);
  // Whether the page is context eligible with account.
  bool (*IsPageContextEligibleWithAccount)(
      const std::string& host,
      const std::string& path,
      const std::string& account,
      const std::vector<optimization_guide::FrameMetadata>& frame_metadata);
  // Whether the page context should be reextracted.
  bool (*ShouldReextractPageContext)(
      const std::string& host,
      const std::string& path,
      const std::vector<std::string>& updated_meta_tags);
  // Returns the meta tag names that, if changed on a frame, could affect the
  // page context eligibility. This could be empty if meta tag changes would
  // not affect eligibility.
  StringViewSpan (*GetMetaTagNamesAffectingEligibility)(
      std::string_view host,
      std::string_view path,
      const std::vector<FrameMetadata>& frame_metadata);
};

// Signature of the GetPageContextEligibilityAPI() function which the shared
// library exports.
using PageContextEligibilityAPIGetter = const PageContextEligibilityAPI* (*)();

}  // namespace optimization_guide

}  // extern "C"

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_API_H_
