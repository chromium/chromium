// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/renderer/paint_preview_recorder_utils.h"

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/unguessable_token.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_recorder.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom-shared.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "components/paint_preview/common/serialized_recording.h"
#include "components/paint_preview/common/test_utils.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace paint_preview {

namespace {

sk_sp<cc::PaintRecord> AddLink(const std::string& link, const SkRect& rect) {
  cc::PaintRecorder link_recorder;
  cc::PaintCanvas* link_canvas = link_recorder.beginRecording(
      rect.x() + rect.width(), rect.y() + rect.height());
  link_canvas->Annotate(cc::PaintCanvas::AnnotationType::URL, rect,
                        SkData::MakeWithCString(link.c_str()));
  return link_recorder.finishRecordingAsPicture();
}

}  // namespace

TEST(PaintPreviewRecorderUtilsTest, TestParseGlyphs) {
  auto typeface = SkTypeface::MakeDefault();
  SkFont font(typeface);
  std::string unichars_1 = "abc";
  std::string unichars_2 = "efg";
  auto blob_1 = SkTextBlob::MakeFromString(unichars_1.c_str(), font);
  auto blob_2 = SkTextBlob::MakeFromString(unichars_2.c_str(), font);

  cc::PaintFlags flags;
  cc::PaintRecorder outer_recorder;
  cc::PaintCanvas* outer_canvas = outer_recorder.beginRecording(100, 100);
  outer_canvas->drawTextBlob(blob_1, 10, 10, flags);
  cc::PaintRecorder inner_recorder;
  cc::PaintCanvas* inner_canvas = inner_recorder.beginRecording(50, 50);
  inner_canvas->drawTextBlob(blob_2, 15, 20, flags);
  outer_canvas->drawPicture(inner_recorder.finishRecordingAsPicture());
  auto record = outer_recorder.finishRecordingAsPicture();

  PaintPreviewTracker tracker(base::UnguessableToken::Create(),
                              base::UnguessableToken::Create(), true);
  ParseGlyphsAndLinks(record.get(), &tracker);
  auto* usage_map = tracker.GetTypefaceUsageMap();
  EXPECT_TRUE(usage_map->count(typeface->uniqueID()));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('a')));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('b')));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('c')));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('e')));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('f')));
  EXPECT_TRUE(
      (*usage_map)[typeface->uniqueID()]->IsSet(typeface->unicharToGlyph('g')));
}

TEST(PaintPreviewRecorderUtilsTest, TestParseLinks) {
  cc::PaintFlags flags;
  cc::PaintRecorder outer_recorder;
  cc::PaintCanvas* outer_canvas = outer_recorder.beginRecording(500, 500);

  outer_canvas->save();
  outer_canvas->translate(10, 20);
  std::string link_1 = "http://www.foo.com/";
  SkRect rect_1 = SkRect::MakeXYWH(10, 20, 30, 40);
  outer_canvas->drawPicture(AddLink(link_1, rect_1));
  outer_canvas->restore();

  outer_canvas->save();
  outer_canvas->concat(SkMatrix::Translate(40, 50));
  outer_canvas->scale(2, 4);
  std::string link_2 = "http://www.bar.com/";
  SkRect rect_2 = SkRect::MakeXYWH(1, 2, 3, 4);
  outer_canvas->drawPicture(AddLink(link_2, rect_2));

  cc::PaintRecorder inner_recorder;
  cc::PaintCanvas* inner_canvas = inner_recorder.beginRecording(500, 500);
  inner_canvas->rotate(20);
  std::string link_3 = "http://www.baz.com/";
  SkRect rect_3 = SkRect::MakeXYWH(5, 7, 9, 13);
  inner_canvas->drawPicture(AddLink(link_3, rect_3));

  outer_canvas->drawPicture(inner_recorder.finishRecordingAsPicture());
  outer_canvas->restore();

  outer_canvas->save();
  outer_canvas->translate(20, 50);
  outer_canvas->setMatrix(SkMatrix::Translate(10, 30));
  std::string link_4 = "http://www.example.com/";
  SkRect rect_4 = SkRect::MakeXYWH(10, 30, 40, 50);
  outer_canvas->drawPicture(AddLink(link_4, rect_4));
  outer_canvas->restore();

  outer_canvas->saveLayer(&rect_1, nullptr);
  outer_canvas->saveLayerAlpha(&rect_1, 8);
  outer_canvas->restoreToCount(1);
  auto record = outer_recorder.finishRecordingAsPicture();

  PaintPreviewTracker tracker(base::UnguessableToken::Create(),
                              base::UnguessableToken::Create(), true);
  ParseGlyphsAndLinks(record.get(), &tracker);

  std::vector<mojom::LinkDataPtr> links;
  tracker.MoveLinks(&links);
  ASSERT_EQ(links.size(), 4U);
  EXPECT_EQ(links[0]->url, link_1);
  EXPECT_EQ(links[0]->rect.x(), rect_1.x() + 10);
  EXPECT_EQ(links[0]->rect.y(), rect_1.y() + 20);
  EXPECT_EQ(links[0]->rect.width(), rect_1.width());
  EXPECT_EQ(links[0]->rect.height(), rect_1.height());

  EXPECT_EQ(links[1]->url, link_2);
  EXPECT_EQ(links[1]->rect.x(), rect_2.x() * 2 + 40);
  EXPECT_EQ(links[1]->rect.y(), rect_2.y() * 4 + 50);
  EXPECT_EQ(links[1]->rect.width(), rect_2.width() * 2);
  EXPECT_EQ(links[1]->rect.height(), rect_2.height() * 4);

  EXPECT_EQ(links[2]->url, link_3);
  EXPECT_EQ(links[2]->rect.x(), 35);
  EXPECT_EQ(links[2]->rect.y(), 83);
  EXPECT_EQ(links[2]->rect.width(), 25);
  EXPECT_EQ(links[2]->rect.height(), 61);

  EXPECT_EQ(links[3]->url, link_4);
  EXPECT_EQ(links[3]->rect.x(), rect_4.x() + 10);
  EXPECT_EQ(links[3]->rect.y(), rect_4.y() + 30);
  EXPECT_EQ(links[3]->rect.width(), rect_4.width());
  EXPECT_EQ(links[3]->rect.height(), rect_4.height());
}

TEST(PaintPreviewRecorderUtilsTest, TestTransformSubframeRects) {
  PaintPreviewTracker tracker(base::UnguessableToken::Create(),
                              base::UnguessableToken::Create(), true);
  gfx::Rect rect(20, 30, 40, 50);
  auto subframe_token = base::UnguessableToken::Create();
  int old_id = tracker.CreateContentForRemoteFrame(rect, subframe_token);

  cc::PaintFlags flags;
  cc::PaintRecorder recorder;
  cc::PaintCanvas* canvas = recorder.beginRecording(500, 500);
  canvas->save();
  canvas->translate(10, 20);
  canvas->recordCustomData(old_id);
  canvas->restore();
  auto record = recorder.finishRecordingAsPicture();

  auto map = tracker.GetSubframePicsForTesting();
  auto it = map.find(old_id);
  ASSERT_NE(it, map.end());
  auto old_cull_rect = it->second->cullRect();
  EXPECT_EQ(rect.x(), old_cull_rect.x());
  EXPECT_EQ(rect.y(), old_cull_rect.y());
  EXPECT_EQ(rect.width(), old_cull_rect.width());
  EXPECT_EQ(rect.height(), old_cull_rect.height());

  ParseGlyphsAndLinks(record.get(), &tracker);

  auto* picture_ctx = tracker.GetPictureSerializationContext();
  ASSERT_EQ(picture_ctx->content_id_to_transformed_clip.size(), 1U);
  auto clip_it = picture_ctx->content_id_to_transformed_clip.find(old_id);
  ASSERT_NE(clip_it, picture_ctx->content_id_to_transformed_clip.end());

  SkRect new_cull_rect = clip_it->second;
  EXPECT_EQ(rect.x() + 10, new_cull_rect.x());
  EXPECT_EQ(rect.y() + 20, new_cull_rect.y());
  EXPECT_EQ(rect.width(), new_cull_rect.width());
  EXPECT_EQ(rect.height(), new_cull_rect.height());
}

class PaintPreviewRecorderUtilsSerializeAsSkPictureTest
    : public testing::TestWithParam<RecordingPersistence> {
 public:
  PaintPreviewRecorderUtilsSerializeAsSkPictureTest()
      : tracker(base::UnguessableToken::Create(),
                base::UnguessableToken::Create(),
                true),
        dimensions(100, 100),
        recorder() {}

  ~PaintPreviewRecorderUtilsSerializeAsSkPictureTest() override = default;

 protected:
  void SetUp() override {
    base::DiscardableMemoryAllocator::SetInstance(&test_allocator_);

    canvas = recorder.beginRecording(dimensions.width(), dimensions.width());
    cc::PaintFlags flags;
    canvas->drawRect(SkRect::MakeWH(dimensions.width(), dimensions.height()),
                     flags);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(dimensions.width(), dimensions.height());
    {
      SkCanvas sk_canvas(bitmap);
      sk_canvas.drawColor(SK_ColorRED);
    }
    canvas->drawImage(cc::PaintImage::CreateFromBitmap(bitmap), 0, 0);
  }

  void TearDown() override {
    base::DiscardableMemoryAllocator::SetInstance(nullptr);
  }

  base::Optional<SerializedRecording> SerializeAsSkPicture(
      base::Optional<size_t> max_capture_size,
      size_t* serialized_size) {
    auto skp = PaintRecordToSkPicture(recorder.finishRecordingAsPicture(),
                                      &tracker, dimensions);
    if (!skp)
      return base::nullopt;

    canvas = nullptr;

    switch (GetParam()) {
      case RecordingPersistence::kFileSystem: {
        base::ScopedTempDir temp_dir;
        if (!temp_dir.CreateUniqueTempDir())
          return base::nullopt;

        base::FilePath file_path = temp_dir.GetPath().AppendASCII("test_file");
        base::File write_file(
            file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
        if (!RecordToFile(std::move(write_file), skp, &tracker,
                          max_capture_size, serialized_size))
          return base::nullopt;

        return {SerializedRecording(file_path)};
      } break;

      case RecordingPersistence::kMemoryBuffer: {
        base::Optional<mojo_base::BigBuffer> buffer =
            RecordToBuffer(skp, &tracker, max_capture_size, serialized_size);
        if (!buffer.has_value())
          return base::nullopt;

        return {SerializedRecording(std::move(buffer.value()))};
      } break;
    }

    NOTREACHED();
    return base::nullopt;
  }

  PaintPreviewTracker tracker;

  gfx::Rect dimensions;
  cc::PaintRecorder recorder;

  // Valid after SetUp() until SerializeAsSkPicture() is called.
  cc::PaintCanvas* canvas{};

 private:
  base::TestDiscardableMemoryAllocator test_allocator_;
};

TEST_P(PaintPreviewRecorderUtilsSerializeAsSkPictureTest, Roundtrip) {
  base::flat_set<uint32_t> ctx;
  uint32_t content_id = tracker.CreateContentForRemoteFrame(
      gfx::Rect(10, 10), base::UnguessableToken::Create());
  canvas->recordCustomData(content_id);
  tracker.TransformClipForFrame(content_id);
  ctx.insert(content_id);

  content_id = tracker.CreateContentForRemoteFrame(
      gfx::Rect(20, 20), base::UnguessableToken::Create());
  canvas->recordCustomData(content_id);
  tracker.TransformClipForFrame(content_id);
  ctx.insert(content_id);

  size_t out_size = 0;
  auto recording = SerializeAsSkPicture(base::nullopt, &out_size);
  ASSERT_TRUE(recording.has_value());

  base::Optional<SkpResult> result = std::move(recording.value()).Deserialize();
  ASSERT_TRUE(result.has_value());
  for (auto& content_id : ctx) {
    EXPECT_TRUE(result->ctx.contains(content_id));
    result->ctx.erase(content_id);
  }
  EXPECT_TRUE(result->ctx.empty());

  SkBitmap bitmap;
  bitmap.allocN32Pixels(dimensions.width(), dimensions.height());
  SkCanvas sk_canvas(bitmap);
  sk_canvas.drawPicture(result->skp);
  EXPECT_EQ(bitmap.getColor(10, 10), SK_ColorRED);
}

TEST_P(PaintPreviewRecorderUtilsSerializeAsSkPictureTest, FailIfExceedMaxSize) {
  size_t out_size = 2;
  auto recording = SerializeAsSkPicture({1}, &out_size);
  EXPECT_FALSE(recording.has_value());
  EXPECT_LE(out_size, 1U);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PaintPreviewRecorderUtilsSerializeAsSkPictureTest,
                         testing::Values(RecordingPersistence::kFileSystem,
                                         RecordingPersistence::kMemoryBuffer),
                         PersistenceParamToString);

TEST(PaintPreviewRecorderUtilsTest, TestBuildResponse) {
  auto token = base::UnguessableToken::Create();
  auto embedding_token = base::UnguessableToken::Create();
  PaintPreviewTracker tracker(token, embedding_token, true);
  tracker.AnnotateLink(GURL("www.google.com"), SkRect::MakeXYWH(1, 2, 3, 4));
  tracker.AnnotateLink(GURL("www.chromium.org"),
                       SkRect::MakeXYWH(10, 20, 10, 20));
  tracker.CreateContentForRemoteFrame(gfx::Rect(1, 1, 1, 1),
                                      base::UnguessableToken::Create());
  tracker.CreateContentForRemoteFrame(gfx::Rect(1, 2, 4, 8),
                                      base::UnguessableToken::Create());

  auto response = mojom::PaintPreviewCaptureResponse::New();
  BuildResponse(&tracker, response.get());

  EXPECT_EQ(response->embedding_token, embedding_token);
  EXPECT_EQ(response->links.size(), 2U);
  EXPECT_THAT(response->links[0]->url, GURL("www.google.com"));
  EXPECT_THAT(response->links[0]->rect, gfx::Rect(1, 2, 3, 4));
  EXPECT_THAT(response->links[1]->url, GURL("www.chromium.org"));
  EXPECT_THAT(response->links[1]->rect, gfx::Rect(10, 20, 10, 20));

  auto* picture_ctx = tracker.GetPictureSerializationContext();
  for (const auto& id_pair : response->content_id_to_embedding_token) {
    auto it = picture_ctx->content_id_to_embedding_token.find(id_pair.first);
    EXPECT_NE(it, picture_ctx->content_id_to_embedding_token.end());
    EXPECT_EQ(id_pair.first, it->first);
    EXPECT_EQ(id_pair.second, it->second);
  }
}

}  // namespace paint_preview
