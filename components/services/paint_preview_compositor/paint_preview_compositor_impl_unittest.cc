// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/paint_preview_compositor/paint_preview_compositor_impl.h"

#include <stdint.h>

#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/serial_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace paint_preview {

namespace {

// Checks that |status| == |expected_status|. If |expected_status| == kSuccess,
// then also checks that;
// - |response->root_frame_guid| == |expected_root_frame_guid|
// - |response->subframe_rect_hierarchy| == |expected_data|
void BeginCompositeCallbackImpl(
    mojom::PaintPreviewCompositor::Status expected_status,
    uint64_t expected_root_frame_guid,
    const base::flat_map<uint64_t, mojom::FrameDataPtr>& expected_data,
    mojom::PaintPreviewCompositor::Status status,
    mojom::PaintPreviewBeginCompositeResponsePtr response) {
  EXPECT_EQ(status, expected_status);
  if (expected_status != mojom::PaintPreviewCompositor::Status::kSuccess)
    return;
  EXPECT_EQ(response->root_frame_guid, expected_root_frame_guid);
  EXPECT_EQ(response->frames.size(), expected_data.size());
  for (const auto& frame : expected_data) {
    EXPECT_TRUE(response->frames.count(frame.first));
    EXPECT_EQ(response->frames[frame.first]->scroll_extents,
              frame.second->scroll_extents);
    size_t size = response->frames[frame.first]->subframes.size();
    EXPECT_EQ(size, frame.second->subframes.size());
    std::vector<std::pair<uint64_t, gfx::Rect>> response_subframes,
        expected_subframes;
    for (size_t i = 0; i < size; ++i) {
      response_subframes.push_back(
          {response->frames[frame.first]->subframes[i]->frame_guid,
           response->frames[frame.first]->subframes[i]->clip_rect});
      expected_subframes.push_back({frame.second->subframes[i]->frame_guid,
                                    frame.second->subframes[i]->clip_rect});
    }
    EXPECT_THAT(response_subframes,
                ::testing::UnorderedElementsAreArray(expected_subframes));
  }
}

// Checks that |status| == |expected_status|. If |expected_status| == kSuccess,
// then it also checks that |bitmap| and |expected_bitmap| are pixel identical.
void BitmapCallbackImpl(mojom::PaintPreviewCompositor::Status expected_status,
                        const SkBitmap& expected_bitmap,
                        mojom::PaintPreviewCompositor::Status status,
                        const SkBitmap& bitmap) {
  EXPECT_EQ(status, expected_status);
  if (expected_status != mojom::PaintPreviewCompositor::Status::kSuccess)
    return;
  EXPECT_EQ(bitmap.width(), expected_bitmap.width());
  EXPECT_EQ(bitmap.height(), expected_bitmap.height());
  EXPECT_EQ(bitmap.bytesPerPixel(), expected_bitmap.bytesPerPixel());
  // Assert that all the bytes of the backing memory are equal. This check is
  // only safe if all of the width, height and bytesPerPixel are equal between
  // the two bitmaps.
  EXPECT_EQ(memcmp(bitmap.getPixels(), expected_bitmap.getPixels(),
                   expected_bitmap.bytesPerPixel() * expected_bitmap.width() *
                       expected_bitmap.height()),
            0);
}

// Encodes |proto| a ReadOnlySharedMemoryRegion.
base::ReadOnlySharedMemoryRegion ToReadOnlySharedMemory(
    const PaintPreviewProto& proto) {
  auto region = base::WritableSharedMemoryRegion::Create(proto.ByteSizeLong());
  EXPECT_TRUE(region.IsValid());
  auto mapping = region.Map();
  EXPECT_TRUE(mapping.IsValid());
  proto.SerializeToArray(mapping.memory(), mapping.size());
  return base::WritableSharedMemoryRegion::ConvertToReadOnly(std::move(region));
}

SkRect ToSkRect(const gfx::Size& size) {
  return SkRect::MakeWH(size.width(), size.height());
}

SkRect ToSkRect(const gfx::Rect& rect) {
  return SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height());
}

void PopulateFrameProto(
    PaintPreviewFrameProto* frame,
    uint64_t id,
    bool set_is_main_frame,
    const base::FilePath& path,
    const gfx::Size& scroll_extents,
    std::vector<std::pair<uint64_t, gfx::Rect>> subframes,
    base::flat_map<uint64_t, base::File>* file_map,
    base::flat_map<uint64_t, mojom::FrameDataPtr>* expected_data) {
  frame->set_id(id);
  frame->set_is_main_frame(set_is_main_frame);

  FileWStream wstream(base::File(
      path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE));
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(ToSkRect(scroll_extents));
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorDKGRAY);
  canvas->drawRect(ToSkRect(scroll_extents), paint);

  PictureSerializationContext picture_context;
  auto* cid_pid_map = frame->mutable_content_id_proxy_id_map();

  mojom::FrameDataPtr frame_data = mojom::FrameData::New();
  frame_data->scroll_extents = scroll_extents;

  for (const auto& subframe : subframes) {
    uint64_t subframe_id = subframe.first;
    gfx::Rect clip_rect = subframe.second;
    sk_sp<SkPicture> temp = SkPicture::MakePlaceholder(ToSkRect(clip_rect));
    cid_pid_map->insert({temp->uniqueID(), subframe_id});
    picture_context.insert({temp->uniqueID(), subframe_id});
    canvas->drawPicture(temp.get());
    frame_data->subframes.push_back(
        mojom::SubframeClipRect::New(subframe_id, clip_rect));
  }

  sk_sp<SkPicture> pic = recorder.finishRecordingAsPicture();

  // nullptr is safe only because no typefaces are serialized.
  SkSerialProcs procs = MakeSerialProcs(&picture_context, nullptr);
  pic->serialize(&wstream, &procs);
  file_map->insert(
      {id, base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ)});
  expected_data->insert({id, std::move(frame_data)});
}

}  // namespace

TEST(PaintPreviewCompositorTest, TestBeginComposite) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  GURL url("https://www.chromium.org");
  PaintPreviewCompositorImpl compositor(mojo::NullReceiver(),
                                        base::BindOnce([]() {}));
  compositor.SetRootFrameUrl(url);

  const uint64_t kRootFrameID = 1;
  gfx::Size root_frame_scroll_extent(100, 200);
  const uint64_t kSubframe_0_ID = 2;
  gfx::Size subframe_0_scroll_extent(50, 75);
  gfx::Rect subframe_0_clip_rect(10, 20, 30, 40);
  const uint64_t kSubframe_0_0_ID = 3;
  gfx::Size subframe_0_0_scroll_extent(20, 20);
  gfx::Rect subframe_0_0_clip_rect(10, 10, 20, 20);
  const uint64_t kSubframe_0_1_ID = 4;
  gfx::Size subframe_0_1_scroll_extent(10, 5);
  gfx::Rect subframe_0_1_clip_rect(10, 10, 30, 30);
  const uint64_t kSubframe_1_ID = 5;
  gfx::Size subframe_1_scroll_extent(1, 1);
  gfx::Rect subframe_1_clip_rect(0, 0, 1, 1);

  PaintPreviewProto proto;
  base::flat_map<uint64_t, base::File> file_map;
  base::flat_map<uint64_t, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(proto.mutable_root_frame(), kRootFrameID, true,
                     temp_dir.GetPath().AppendASCII("root.skp"),
                     root_frame_scroll_extent,
                     {{kSubframe_0_ID, subframe_0_clip_rect},
                      {kSubframe_1_ID, subframe_1_clip_rect}},
                     &file_map, &expected_data);
  PopulateFrameProto(proto.add_subframes(), kSubframe_0_ID, false,
                     temp_dir.GetPath().AppendASCII("subframe_0.skp"),
                     subframe_0_scroll_extent,
                     {{kSubframe_0_0_ID, subframe_0_0_clip_rect},
                      {kSubframe_0_1_ID, subframe_0_1_clip_rect}},
                     &file_map, &expected_data);
  PopulateFrameProto(proto.add_subframes(), kSubframe_0_0_ID, false,
                     temp_dir.GetPath().AppendASCII("subframe_0_0.skp"),
                     subframe_0_0_scroll_extent, {}, &file_map, &expected_data);
  PopulateFrameProto(proto.add_subframes(), kSubframe_0_1_ID, false,
                     temp_dir.GetPath().AppendASCII("subframe_0_1.skp"),
                     subframe_0_1_scroll_extent, {}, &file_map, &expected_data);
  PopulateFrameProto(proto.add_subframes(), kSubframe_1_ID, false,
                     temp_dir.GetPath().AppendASCII("subframe_1.skp"),
                     subframe_1_scroll_extent, {}, &file_map, &expected_data);
  // Missing a subframe SKP is still valid. Compositing will ignore it in the
  // results.
  file_map.erase(kSubframe_0_0_ID);
  expected_data.erase(kSubframe_0_0_ID);

  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  request->file_map = std::move(file_map);
  request->proto = ToReadOnlySharedMemory(proto);

  compositor.BeginComposite(
      std::move(request),
      base::BindOnce(&BeginCompositeCallbackImpl,
                     mojom::PaintPreviewCompositor::Status::kSuccess,
                     kRootFrameID, std::move(expected_data)));
}

// Ensure that depending on a frame multiple times works.
TEST(PaintPreviewCompositorTest, TestBeginCompositeDuplicate) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  PaintPreviewCompositorImpl compositor(mojo::NullReceiver(),
                                        base::BindOnce([]() {}));

  const uint64_t kRootFrameID = 1;
  gfx::Size root_frame_scroll_extent(100, 200);
  const uint64_t kSubframe_0_ID = 2;
  gfx::Size subframe_0_scroll_extent(50, 75);
  gfx::Rect subframe_0_clip_rect(10, 20, 30, 40);

  PaintPreviewProto proto;
  base::flat_map<uint64_t, base::File> file_map;
  base::flat_map<uint64_t, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(proto.mutable_root_frame(), kRootFrameID, true,
                     temp_dir.GetPath().AppendASCII("root.skp"),
                     root_frame_scroll_extent,
                     {{kSubframe_0_ID, subframe_0_clip_rect},
                      {kSubframe_0_ID, subframe_0_clip_rect}},
                     &file_map, &expected_data);
  PopulateFrameProto(proto.add_subframes(), kSubframe_0_ID, false,
                     temp_dir.GetPath().AppendASCII("subframe_0.skp"),
                     subframe_0_scroll_extent, {}, &file_map, &expected_data);

  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  request->file_map = std::move(file_map);
  request->proto = ToReadOnlySharedMemory(proto);

  compositor.BeginComposite(
      std::move(request),
      base::BindOnce(&BeginCompositeCallbackImpl,
                     mojom::PaintPreviewCompositor::Status::kSuccess,
                     kRootFrameID, std::move(expected_data)));
}

// Ensure that a loop in frames works.
TEST(PaintPreviewCompositorTest, TestBeginCompositeLoop) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  PaintPreviewCompositorImpl compositor(mojo::NullReceiver(),
                                        base::BindOnce([]() {}));

  const uint64_t kRootFrameID = 1;
  gfx::Size root_frame_scroll_extent(100, 200);
  const uint64_t kSubframe_0_ID = 2;
  gfx::Size subframe_0_scroll_extent(50, 75);
  gfx::Rect subframe_0_clip_rect(10, 20, 30, 40);

  PaintPreviewProto proto;
  base::flat_map<uint64_t, base::File> file_map;
  base::flat_map<uint64_t, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(
      proto.mutable_root_frame(), kRootFrameID, true,
      temp_dir.GetPath().AppendASCII("root.skp"), root_frame_scroll_extent,
      {{kSubframe_0_ID, subframe_0_clip_rect}}, &file_map, &expected_data);
  PopulateFrameProto(proto.add_subframes(), kSubframe_0_ID, false,
                     temp_dir.GetPath().AppendASCII("subframe_0.skp"),
                     subframe_0_scroll_extent,
                     {{kRootFrameID, subframe_0_clip_rect}}, &file_map,
                     &expected_data);

  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  request->file_map = std::move(file_map);
  request->proto = ToReadOnlySharedMemory(proto);

  compositor.BeginComposite(
      std::move(request),
      base::BindOnce(&BeginCompositeCallbackImpl,
                     mojom::PaintPreviewCompositor::Status::kSuccess,
                     kRootFrameID, std::move(expected_data)));
}

// Ensure that a frame referencing itself works correctly.
TEST(PaintPreviewCompositorTest, TestBeginCompositeSelfReference) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  PaintPreviewCompositorImpl compositor(mojo::NullReceiver(),
                                        base::BindOnce([]() {}));

  const uint64_t kRootFrameID = 1;
  gfx::Size root_frame_scroll_extent(100, 200);
  gfx::Rect root_frame_clip_rect(10, 20, 30, 40);

  PaintPreviewProto proto;
  base::flat_map<uint64_t, base::File> file_map;
  base::flat_map<uint64_t, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(
      proto.mutable_root_frame(), kRootFrameID, true,
      temp_dir.GetPath().AppendASCII("root.skp"), root_frame_scroll_extent,
      {{kRootFrameID, root_frame_clip_rect}}, &file_map, &expected_data);

  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  request->file_map = std::move(file_map);
  request->proto = ToReadOnlySharedMemory(proto);

  compositor.BeginComposite(
      std::move(request),
      base::BindOnce(&BeginCompositeCallbackImpl,
                     mojom::PaintPreviewCompositor::Status::kSuccess,
                     kRootFrameID, std::move(expected_data)));
}

TEST(PaintPreviewCompositorTest, TestInvalidRegionHandling) {
  PaintPreviewCompositorImpl compositor(mojo::NullReceiver(),
                                        base::BindOnce([]() {}));

  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  compositor.BeginComposite(
      std::move(request),
      base::BindOnce(
          &BeginCompositeCallbackImpl,
          mojom::PaintPreviewCompositor::Status::kDeserializingFailure, 0,
          base::flat_map<uint64_t, mojom::FrameDataPtr>()));
}

TEST(PaintPreviewCompositorTest, TestInvalidProto) {
  PaintPreviewCompositorImpl compositor(mojo::NullReceiver(),
                                        base::BindOnce([]() {}));

  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  std::string test_data = "hello world";
  auto region = base::WritableSharedMemoryRegion::Create(test_data.size());
  ASSERT_TRUE(region.IsValid());
  auto mapping = region.Map();
  ASSERT_TRUE(mapping.IsValid());
  memcpy(mapping.memory(), test_data.data(), mapping.size());

  // These calls log errors without a newline (from the proto lib). As a
  // result, the Android gtest parser fails to parse the test status. To work
  // around this gobble the log message.
  {
    testing::internal::CaptureStdout();
    request->proto =
        base::WritableSharedMemoryRegion::ConvertToReadOnly(std::move(region));
    compositor.BeginComposite(
        std::move(request),
        base::BindOnce(
            &BeginCompositeCallbackImpl,
            mojom::PaintPreviewCompositor::Status::kDeserializingFailure, 0,
            base::flat_map<uint64_t, mojom::FrameDataPtr>()));
    LOG(ERROR) << testing::internal::GetCapturedStdout();
  }
}

TEST(PaintPreviewCompositorTest, TestInvalidRootFrame) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  PaintPreviewCompositorImpl compositor(mojo::NullReceiver(),
                                        base::BindOnce([]() {}));

  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  PaintPreviewProto proto;
  const uint64_t kRootFrameID = 1;
  base::flat_map<uint64_t, base::File> file_map;
  base::flat_map<uint64_t, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(proto.mutable_root_frame(), kRootFrameID, true,
                     temp_dir.GetPath().AppendASCII("root.skp"),
                     gfx::Size(1, 1), {}, &file_map, &expected_data);
  file_map.erase(kRootFrameID);  // Missing a SKP for the root file is invalid.
  request->file_map = std::move(file_map);
  request->proto = ToReadOnlySharedMemory(proto);
  compositor.BeginComposite(
      std::move(request),
      base::BindOnce(&BeginCompositeCallbackImpl,
                     mojom::PaintPreviewCompositor::Status::kCompositingFailure,
                     0, base::flat_map<uint64_t, mojom::FrameDataPtr>()));
}

TEST(PaintPreviewCompositorTest, TestComposite) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  PaintPreviewCompositorImpl compositor(mojo::NullReceiver(),
                                        base::BindOnce([]() {}));
  const uint64_t kRootFrameID = 1;
  gfx::Size root_frame_scroll_extent(100, 200);
  PaintPreviewProto proto;
  base::flat_map<uint64_t, base::File> file_map;
  base::flat_map<uint64_t, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(proto.mutable_root_frame(), kRootFrameID, true,
                     temp_dir.GetPath().AppendASCII("root.skp"),
                     root_frame_scroll_extent, {}, &file_map, &expected_data);
  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  request->file_map = std::move(file_map);
  request->proto = ToReadOnlySharedMemory(proto);
  compositor.BeginComposite(
      std::move(request),
      base::BindOnce(&BeginCompositeCallbackImpl,
                     mojom::PaintPreviewCompositor::Status::kSuccess,
                     kRootFrameID, std::move(expected_data)));
  gfx::Rect rect(200, 400);
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(rect.width(), rect.height()));
  SkCanvas canvas(bitmap);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorDKGRAY);
  canvas.drawRect(ToSkRect(rect), paint);
  compositor.BitmapForFrame(
      kRootFrameID, rect, 2,
      base::BindOnce(&BitmapCallbackImpl,
                     mojom::PaintPreviewCompositor::Status::kSuccess, bitmap));

  compositor.BitmapForFrame(
      kRootFrameID + 1, rect, 2,
      base::BindOnce(&BitmapCallbackImpl,
                     mojom::PaintPreviewCompositor::Status::kCompositingFailure,
                     bitmap));
}

}  // namespace paint_preview
