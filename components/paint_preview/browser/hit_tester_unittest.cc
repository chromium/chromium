// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/hit_tester.h"

#include <vector>

#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace paint_preview {

// This test suite is *not* an extensive test of the underlying rtree it only
// aims to test the Build() options.

TEST(PaintPreviewHitTesterTest, TestBuildLinkData) {
  HitTester hit_tester;
  std::vector<LinkData> links;
  GURL url("https://foo.com");
  links.push_back({gfx::Rect(50, 50, 50, 50), url});
  hit_tester.Build(links);
  EXPECT_TRUE(hit_tester.IsValid());

  std::vector<const GURL*> urls;
  hit_tester.HitTest(gfx::Rect(75, 75, 1, 1), &urls);
  EXPECT_EQ(urls.size(), 1U);
  EXPECT_EQ(*urls[0], url);

  urls.clear();
  hit_tester.HitTest(gfx::Rect(1, 1, 1, 1), &urls);
  EXPECT_TRUE(urls.empty());
}

TEST(PaintPreviewHitTesterTest, TestBuildPaintPreviewFrameProto) {
  HitTester hit_tester;
  PaintPreviewFrameProto proto;
  auto* link = proto.add_links();
  link->set_url("http://baz.io");
  auto* rect = link->mutable_rect();
  rect->set_x(1);
  rect->set_y(1);
  rect->set_width(10);
  rect->set_height(10);
  hit_tester.Build(proto);
  EXPECT_TRUE(hit_tester.IsValid());

  std::vector<const GURL*> urls;
  hit_tester.HitTest(gfx::Rect(5, 5, 1, 1), &urls);
  EXPECT_EQ(urls.size(), 1U);
  EXPECT_EQ(*urls[0], GURL(link->url()));

  urls.clear();
  hit_tester.HitTest(gfx::Rect(0, 0, 1, 1), &urls);
  EXPECT_TRUE(urls.empty());
}

}  // namespace paint_preview
