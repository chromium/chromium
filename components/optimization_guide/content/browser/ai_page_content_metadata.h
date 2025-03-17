// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_AI_PAGE_CONTENT_METADATA_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_AI_PAGE_CONTENT_METADATA_H_

#include <string>
#include <vector>
#include "url/gurl.h"

namespace optimization_guide {

struct MetaTag {
  std::string name;
  std::string content;
};

// Metadata about a frame. Includes the URL and meta tags.
struct FrameMetadata {
  FrameMetadata();
  FrameMetadata(const FrameMetadata& other) = delete;
  FrameMetadata(FrameMetadata&& other);
  FrameMetadata& operator=(const FrameMetadata& other) = delete;
  FrameMetadata& operator=(FrameMetadata&& other);
  ~FrameMetadata();

  GURL url;
  std::vector<MetaTag> meta_tags;
};

// Metadata about the page content.  Includes metadata about each frame.
struct AIPageContentMetadata {
  AIPageContentMetadata();
  AIPageContentMetadata(const AIPageContentMetadata& other) = delete;
  AIPageContentMetadata(AIPageContentMetadata&& other);
  AIPageContentMetadata& operator=(const AIPageContentMetadata& other) = delete;
  AIPageContentMetadata& operator=(AIPageContentMetadata&& other);
  ~AIPageContentMetadata();

  std::vector<FrameMetadata> frame_metadata;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_AI_PAGE_CONTENT_METADATA_H_
