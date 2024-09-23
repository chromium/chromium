// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_HIT_TESTER_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_HIT_TESTER_H_

#include <vector>

#include "cc/base/rtree.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace paint_preview {

// Struct for holding simple link data.
struct LinkData {
  gfx::Rect rect;
  GURL url;
};

// A class for hit testing to find the locations of links in a paint preview.
class HitTester {
 public:
  HitTester();
  ~HitTester();

  HitTester(const HitTester&) = delete;
  HitTester& operator=(const HitTester&) = delete;

  // Builds a R-Tree from the underlying data.
  void Build(const PaintPreviewFrameProto& proto);
  void Build(const std::vector<LinkData>& links);

  // Returns false if the underlying rtree is not valid.
  bool IsValid();

  // Finds all rects in the provided data that intersect with query and returns
  // a vector of non-owning pointers to corresponding GURLs.
  void HitTest(const gfx::Rect& query, std::vector<const GURL*>* results) const;

 private:
  cc::RTree<GURL> rtree_;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_HIT_TESTER_H_
