// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/hit_tester.h"

#include <vector>

#include "components/paint_preview/common/proto/paint_preview.pb.h"

namespace paint_preview {

HitTester::HitTester() = default;
HitTester::~HitTester() = default;

void HitTester::Build(const PaintPreviewFrameProto& proto) {
  const auto& links = proto.links();
  rtree_.Build(
      links.size(),
      [&links](size_t index) {
        const auto& rect = links[index].rect();
        return gfx::Rect(rect.x(), rect.y(), rect.width(), rect.height());
      },
      [&links](size_t index) { return GURL(links[index].url()); });
}

void HitTester::Build(const std::vector<LinkData>& links) {
  rtree_.Build(
      links.size(), [&links](size_t index) { return links[index].rect; },
      [&links](size_t index) { return links[index].url; });
}

bool HitTester::IsValid() {
  return rtree_.has_valid_bounds();
}

void HitTester::HitTest(const gfx::Rect& query,
                        std::vector<const GURL*>* results) const {
  rtree_.SearchRefs(query, results);
}

}  // namespace paint_preview
