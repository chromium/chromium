// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/hit_tester.h"

#include <vector>

#include "components/paint_preview/common/proto/paint_preview.pb.h"

namespace paint_preview {

namespace {

LinkData ToLinkData(const LinkDataProto& proto) {
  return {gfx::Rect(proto.rect().x(), proto.rect().y(), proto.rect().width(),
                    proto.rect().height()),
          GURL(proto.url())};
}

gfx::Rect BoundsGetter(const std::vector<LinkData>& links, size_t index) {
  return links[index].rect;
}

GURL PayloadGetter(const std::vector<LinkData>& links, size_t index) {
  return links[index].url;
}

}  // namespace

HitTester::HitTester() = default;
HitTester::~HitTester() = default;

void HitTester::Build(const PaintPreviewFrameProto& proto) {
  std::vector<LinkData> link_data;
  link_data.reserve(proto.links_size());
  for (const auto& link : proto.links())
    link_data.push_back(ToLinkData(link));
  Build(link_data);
}
void HitTester::Build(const std::vector<LinkDataProto>& links) {
  std::vector<LinkData> link_data;
  link_data.reserve(links.size());
  for (const auto& link : links)
    link_data.push_back(ToLinkData(link));
  Build(link_data);
}
void HitTester::Build(const std::vector<LinkData>& links) {
  rtree_.Build(links, &BoundsGetter, &PayloadGetter);
}

bool HitTester::IsValid() {
  return rtree_.has_valid_bounds();
}

void HitTester::HitTest(const gfx::Rect& query,
                        std::vector<const GURL*>* results) const {
  rtree_.SearchRefs(query, results);
}

void HitTester::Reset() {
  rtree_.Reset();
}

}  // namespace paint_preview
