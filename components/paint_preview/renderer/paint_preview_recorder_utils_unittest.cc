// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/renderer/paint_preview_recorder_utils.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/unguessable_token.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_recorder.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "components/paint_preview/common/serial_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace paint_preview {

namespace {

constexpr int32_t kRoutingId = 1;

}  // namespace

TEST(PaintPreviewServiceUtilsTest, TestParseGlyphs) {
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

  PaintPreviewTracker tracker(base::UnguessableToken::Create(), kRoutingId,
                              true);
  ParseGlyphs(record.get(), &tracker);
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

TEST(PaintPreviewServiceUtilsTest, TestSerializeAsSkPicture) {
  PaintPreviewTracker tracker(base::UnguessableToken::Create(), kRoutingId,
                              true);

  gfx::Rect dimensions(100, 100);
  cc::PaintRecorder recorder;
  cc::PaintCanvas* canvas =
      recorder.beginRecording(dimensions.width(), dimensions.width());
  cc::PaintFlags flags;
  canvas->drawRect(SkRect::MakeWH(dimensions.width(), dimensions.height()),
                   flags);

  base::flat_set<uint32_t> ctx;
  uint32_t content_id =
      tracker.CreateContentForRemoteFrame(gfx::Rect(10, 10), kRoutingId + 1);
  canvas->recordCustomData(content_id);
  ctx.insert(content_id);
  content_id =
      tracker.CreateContentForRemoteFrame(gfx::Rect(20, 20), kRoutingId + 2);
  canvas->recordCustomData(content_id);
  ctx.insert(content_id);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("test_file");
  base::File write_file(
      file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  auto record = recorder.finishRecordingAsPicture();
  EXPECT_TRUE(SerializeAsSkPicture(record, &tracker, dimensions,
                                   std::move(write_file)));
  base::File read_file(file_path, base::File::FLAG_OPEN |
                                      base::File::FLAG_READ |
                                      base::File::FLAG_EXCLUSIVE_READ);
  FileRStream rstream(std::move(read_file));
  SkDeserialProcs procs;
  procs.fPictureProc = [](const void* data, size_t length, void* ctx) {
    uint32_t content_id;
    if (length < sizeof(content_id))
      return MakeEmptyPicture();
    memcpy(&content_id, data, sizeof(content_id));
    auto* context = reinterpret_cast<base::flat_set<uint32_t>*>(ctx);
    EXPECT_TRUE(context->count(content_id));
    context->erase(content_id);
    return MakeEmptyPicture();
  };
  procs.fPictureCtx = &ctx;
  SkPicture::MakeFromStream(&rstream, &procs);
  EXPECT_TRUE(ctx.empty());
}

TEST(PaintPreviewServiceUtilsTest, TestBuildAndSerializeProto) {
  auto token = base::UnguessableToken::Create();
  PaintPreviewTracker tracker(token, kRoutingId, true);
  tracker.AnnotateLink(GURL("www.google.com"), gfx::Rect(1, 2, 3, 4));
  tracker.AnnotateLink(GURL("www.chromium.org"), gfx::Rect(10, 20, 10, 20));
  tracker.CreateContentForRemoteFrame(gfx::Rect(1, 1, 1, 1), kRoutingId + 1);
  tracker.CreateContentForRemoteFrame(gfx::Rect(1, 2, 4, 8), kRoutingId + 2);

  auto response = mojom::PaintPreviewCaptureResponse::New();
  BuildResponse(&tracker, response.get());

  EXPECT_EQ(static_cast<int>(response->id), kRoutingId);
  EXPECT_EQ(response->links.size(), 2U);
  EXPECT_EQ(response->links.size(), tracker.GetLinks().size());
  for (size_t i = 0; i < response->links.size(); ++i) {
    EXPECT_THAT(response->links[i]->url, tracker.GetLinks()[i].url);
    EXPECT_THAT(response->links[i]->rect, tracker.GetLinks()[i].rect);
  }
  auto* content_map = tracker.GetPictureSerializationContext();
  for (const auto& id_pair : response->content_id_proxy_id_map) {
    auto it = content_map->find(id_pair.first);
    EXPECT_NE(it, content_map->end());
    EXPECT_EQ(id_pair.first, it->first);
    EXPECT_EQ(id_pair.second, it->second);
  }
}

}  // namespace paint_preview
