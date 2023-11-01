// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/paint_preview_tracker.h"

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/common/serial_utils.h"
#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "ui/gfx/geometry/rect.h"

namespace paint_preview {

namespace {

struct TestContext {
  raw_ptr<const gfx::Rect> rect;
  bool was_called;
};

// A test canvas for checking that the pictures drawn to it have the cull rect
// we expect them to.
class ExpectSubframeCanvas : public SkCanvas {
 public:
  void onDrawPicture(const SkPicture* picture,
                     const SkMatrix*,
                     const SkPaint*) override {
    drawn_pictures_.insert({picture->uniqueID(), picture->cullRect()});
  }

  void ExpectHasPicture(uint32_t expected_picture_id,
                        const gfx::Rect& expected_bounds) {
    auto it = drawn_pictures_.find(expected_picture_id);
    if (it == drawn_pictures_.end()) {
      ADD_FAILURE() << "Picture ID was not recorded.";
      return;
    }

    SkIRect rect = it->second.round();
    EXPECT_EQ(rect.x(), expected_bounds.x());
    EXPECT_EQ(rect.y(), expected_bounds.y());
    EXPECT_EQ(rect.width(), expected_bounds.width());
    EXPECT_EQ(rect.height(), expected_bounds.height());
  }

 private:
  // Map of picture id to expected bounds of pictures drawn into this canvas.
  base::flat_map<uint32_t, SkRect> drawn_pictures_;
};

}  // namespace

TEST(PaintPreviewTrackerTest, TestGetters) {
  const base::UnguessableToken kDocToken = base::UnguessableToken::Create();
  const base::UnguessableToken kEmbeddingToken =
      base::UnguessableToken::Create();
  PaintPreviewTracker tracker(kDocToken, kEmbeddingToken, true);
  EXPECT_EQ(tracker.Guid(), kDocToken);
  EXPECT_EQ(tracker.EmbeddingToken(), kEmbeddingToken);
  EXPECT_TRUE(tracker.IsMainFrame());
}

TEST(PaintPreviewTrackerTest, TestRemoteFramePlaceholderPicture) {
  const base::UnguessableToken kDocToken = base::UnguessableToken::Create();
  const base::UnguessableToken kEmbeddingToken =
      base::UnguessableToken::Create();
  PaintPreviewTracker tracker(kDocToken, kEmbeddingToken, true);

  const base::UnguessableToken kEmbeddingTokenChild =
      base::UnguessableToken::Create();
  gfx::Rect rect(50, 40, 30, 20);
  uint32_t content_id =
      tracker.CreateContentForRemoteFrame(rect, kEmbeddingTokenChild);
  PictureSerializationContext* context =
      tracker.GetPictureSerializationContext();
  EXPECT_TRUE(context->content_id_to_embedding_token.count(content_id));
  EXPECT_EQ(context->content_id_to_embedding_token[content_id],
            kEmbeddingTokenChild);

  ExpectSubframeCanvas canvas;
  tracker.CustomDataToSkPictureCallback(&canvas, content_id);

  canvas.ExpectHasPicture(content_id, rect);
}

TEST(PaintPreviewTrackerTest, TestGlyphRunList) {
  const base::UnguessableToken kEmbeddingToken =
      base::UnguessableToken::Create();
  PaintPreviewTracker tracker(base::UnguessableToken::Create(), kEmbeddingToken,
                              true);
  std::string unichars = "abc";
  auto typeface = skia::DefaultTypeface();
  SkFont font(typeface);
  auto blob = SkTextBlob::MakeFromString(unichars.c_str(), font);
  tracker.AddGlyphs(blob.get());
  auto* usage_map = tracker.GetTypefaceUsageMap();
  EXPECT_TRUE(usage_map->count(typeface->uniqueID()));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('a')));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('b')));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('c')));
}

TEST(PaintPreviewTrackerTest, TestAnnotateLinks) {
  const base::UnguessableToken kEmbeddingToken =
      base::UnguessableToken::Create();
  PaintPreviewTracker tracker(base::UnguessableToken::Create(), kEmbeddingToken,
                              true);
  const GURL url_1("https://www.chromium.org");
  const auto rect_1 = SkRect::MakeXYWH(10, 20, 30, 40);
  tracker.AnnotateLink(url_1, rect_1);

  const GURL url_2("https://www.w3.org");
  const auto rect_2 = SkRect::MakeXYWH(15, 25, 35, 45);
  tracker.AnnotateLink(url_2, rect_2);

  ASSERT_EQ(tracker.GetLinks().size(), 2U);

  EXPECT_EQ(tracker.GetLinks()[0]->url, url_1);
  EXPECT_EQ(tracker.GetLinks()[0]->rect.width(), rect_1.width());
  EXPECT_EQ(tracker.GetLinks()[0]->rect.height(), rect_1.height());
  EXPECT_EQ(tracker.GetLinks()[0]->rect.x(), rect_1.x());
  EXPECT_EQ(tracker.GetLinks()[0]->rect.y(), rect_1.y());
  EXPECT_EQ(tracker.GetLinks()[1]->url, url_2);
  EXPECT_EQ(tracker.GetLinks()[1]->rect.width(), rect_2.width());
  EXPECT_EQ(tracker.GetLinks()[1]->rect.height(), rect_2.height());
  EXPECT_EQ(tracker.GetLinks()[1]->rect.x(), rect_2.x());
  EXPECT_EQ(tracker.GetLinks()[1]->rect.y(), rect_2.y());
}

TEST(PaintPreviewTrackerTest, TestAnnotateAndMoveLinks) {
  const base::UnguessableToken kEmbeddingToken =
      base::UnguessableToken::Create();
  PaintPreviewTracker tracker(base::UnguessableToken::Create(), kEmbeddingToken,
                              true);
  const GURL url_1("https://www.chromium.org");
  const auto rect_1 = SkRect::MakeXYWH(10, 20, 30, 40);
  tracker.AnnotateLink(url_1, rect_1);

  const GURL url_2("https://www.w3.org");
  const auto rect_2 = SkRect::MakeXYWH(15, 25, 35, 45);
  tracker.AnnotateLink(url_2, rect_2);

  std::vector<mojom::LinkDataPtr> links;
  tracker.MoveLinks(&links);
  ASSERT_EQ(tracker.GetLinks().size(), 0U);

  ASSERT_EQ(links.size(), 2U);
  EXPECT_EQ(links[0]->url, url_1);
  EXPECT_EQ(links[0]->rect.width(), rect_1.width());
  EXPECT_EQ(links[0]->rect.height(), rect_1.height());
  EXPECT_EQ(links[0]->rect.x(), rect_1.x());
  EXPECT_EQ(links[0]->rect.y(), rect_1.y());
  EXPECT_EQ(links[1]->url, url_2);
  EXPECT_EQ(links[1]->rect.width(), rect_2.width());
  EXPECT_EQ(links[1]->rect.height(), rect_2.height());
  EXPECT_EQ(links[1]->rect.x(), rect_2.x());
  EXPECT_EQ(links[1]->rect.y(), rect_2.y());
}

TEST(PaintPreviewTrackerTest, AnnotateLinksWithTransform) {
  const base::UnguessableToken kEmbeddingToken =
      base::UnguessableToken::Create();
  PaintPreviewTracker tracker(base::UnguessableToken::Create(), kEmbeddingToken,
                              true);

  const GURL url("http://www.chromium.org");
  const auto rect = SkRect::MakeXYWH(10, 20, 30, 40);
  tracker.AnnotateLink(url, rect);

  std::vector<mojom::LinkDataPtr> links;
  tracker.MoveLinks(&links);
  ASSERT_EQ(links.size(), 1U);
  EXPECT_EQ(links[0]->url, url);
  EXPECT_EQ(links[0]->rect.width(), rect.width());
  EXPECT_EQ(links[0]->rect.height(), rect.height());
  EXPECT_EQ(links[0]->rect.x(), rect.x());
  EXPECT_EQ(links[0]->rect.y(), rect.y());

  tracker.Save();
  tracker.Scale(2, 4);
  tracker.AnnotateLink(url, rect);
  links.clear();
  tracker.MoveLinks(&links);
  EXPECT_EQ(links[0]->url, url);
  EXPECT_EQ(links[0]->rect.width(), rect.width() * 2);
  EXPECT_EQ(links[0]->rect.height(), rect.height() * 4);
  EXPECT_EQ(links[0]->rect.x(), rect.x() * 2);
  EXPECT_EQ(links[0]->rect.y(), rect.y() * 4);

  tracker.Translate(10, 20);
  tracker.AnnotateLink(url, rect);
  links.clear();
  tracker.MoveLinks(&links);
  EXPECT_EQ(links[0]->url, url);
  EXPECT_EQ(links[0]->rect.width(), rect.width() * 2);
  EXPECT_EQ(links[0]->rect.height(), rect.height() * 4);
  EXPECT_EQ(links[0]->rect.x(), (10 + rect.x()) * 2);
  EXPECT_EQ(links[0]->rect.y(), (20 + rect.y()) * 4);

  tracker.Restore();
  links.clear();
  tracker.AnnotateLink(url, rect);
  tracker.MoveLinks(&links);
  ASSERT_EQ(links.size(), 1U);
  EXPECT_EQ(links[0]->url, url);
  EXPECT_EQ(links[0]->rect.width(), rect.width());
  EXPECT_EQ(links[0]->rect.height(), rect.height());
  EXPECT_EQ(links[0]->rect.x(), rect.x());
  EXPECT_EQ(links[0]->rect.y(), rect.y());

  tracker.Concat(SkMatrix::Translate(30, 100));
  links.clear();
  tracker.AnnotateLink(url, rect);
  tracker.MoveLinks(&links);
  ASSERT_EQ(links.size(), 1U);
  EXPECT_EQ(links[0]->url, url);
  EXPECT_EQ(links[0]->rect.width(), rect.width());
  EXPECT_EQ(links[0]->rect.height(), rect.height());
  EXPECT_EQ(links[0]->rect.x(), rect.x() + 30);
  EXPECT_EQ(links[0]->rect.y(), rect.y() + 100);

  tracker.Rotate(30);
  links.clear();
  tracker.AnnotateLink(url, rect);
  tracker.MoveLinks(&links);
  ASSERT_EQ(links.size(), 1U);
  EXPECT_EQ(links[0]->url, url);
  EXPECT_EQ(links[0]->rect.width(), 45);
  EXPECT_EQ(links[0]->rect.height(), 49);
  EXPECT_EQ(links[0]->rect.x(), 8);
  EXPECT_EQ(links[0]->rect.y(), 122);

  // no-op (ensure this doesn't crash).
  tracker.Restore();
}

}  // namespace paint_preview
