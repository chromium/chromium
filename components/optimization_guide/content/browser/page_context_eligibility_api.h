// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_API_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_API_H_

#include <string>
#include <vector>

extern "C" {

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

// Table of C API functions defined within the library.
struct PageContextEligibilityAPI {
  // Whether the page is context eligible.
  bool (*IsPageContextEligible)(
      const std::string& host,
      const std::string& path,
      const std::vector<optimization_guide::FrameMetadata>& frame_metadata);
};

// Signature of the GetPageContextEligibilityAPI() function which the shared
// library exports.
using PageContextEligibilityAPIGetter = const PageContextEligibilityAPI* (*)();

}  // namespace optimization_guide

}  // extern "C"

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTEXT_ELIGIBILITY_API_H_
