// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/services/paint_preview_compositor/paint_preview_compositor_impl.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/test/task_environment.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/common/capture_result.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/recording_map.h"
#include "components/paint_preview/common/serialized_recording.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/base/proto_wrapper_passkeys.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace paint_preview {

namespace {

// Checks that |status| == |expected_status|. If |expected_status| == kSuccess,
// then also checks that;
// - |response->root_frame_guid| == |expected_root_frame_guid|
// - |response->subframe_rect_hierarchy| == |expected_data|
void BeginCompositeCallbackImpl(
    mojom::PaintPreviewCompositor::BeginCompositeStatus expected_status,
    const base::UnguessableToken& expected_root_frame_guid,
    const base::flat_map<base::UnguessableToken, mojom::FrameDataPtr>&
        expected_data,
    mojom::PaintPreviewCompositor::BeginCompositeStatus status,
    mojom::PaintPreviewBeginCompositeResponsePtr response) {
  EXPECT_EQ(status, expected_status);
  if (expected_status !=
      mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess)
    return;
  EXPECT_EQ(response->root_frame_guid, expected_root_frame_guid);
  EXPECT_EQ(response->frames.size(), expected_data.size());
  for (const auto& frame : expected_data) {
    EXPECT_TRUE(response->frames.count(frame.first));
    EXPECT_EQ(response->frames[frame.first]->scroll_extents,
              frame.second->scroll_extents);
    size_t size = response->frames[frame.first]->subframes.size();
    EXPECT_EQ(size, frame.second->subframes.size());
    std::vector<std::pair<base::UnguessableToken, gfx::RectF>>
        response_subframes, expected_subframes;
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
void BitmapCallbackImpl(
    mojom::PaintPreviewCompositor::BitmapStatus expected_status,
    const SkBitmap& expected_bitmap,
    mojom::PaintPreviewCompositor::BitmapStatus status,
    const SkBitmap& bitmap) {
  EXPECT_EQ(status, expected_status);
  if (expected_status != mojom::PaintPreviewCompositor::BitmapStatus::kSuccess)
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

SkRect ToSkRect(const gfx::Size& size) {
  return SkRect::MakeWH(size.width(), size.height());
}

// Draw a dummy picture of size |scroll_extents|, whose origin is equal to
// |clip_rect| and clipped by |clip_rect|'s size. The dummy picture will by
// filled with |rect_fill_color| with a cyan border and will have an XY axis
// drawn at the origin in red and green that is 10 units in |canvas|'s local
// coordinate system.
void DrawDummyTestPicture(SkCanvas* canvas,
                          SkColor rect_fill_color,
                          const gfx::Size& scroll_extents,
                          std::optional<gfx::RectF> clip_rect = std::nullopt,
                          gfx::Size scroll_offsets = gfx::Size()) {
  canvas->save();
  if (clip_rect.has_value()) {
    canvas->clipRect(gfx::RectFToSkRect(*clip_rect));
    canvas->translate(clip_rect->x(), clip_rect->y());
  }
  canvas->translate(-scroll_offsets.width(), -scroll_offsets.height());

  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(rect_fill_color);
  canvas->drawRect(ToSkRect(scroll_extents), paint);

  SkPaint border_paint;
  border_paint.setStyle(SkPaint::kStroke_Style);
  border_paint.setColor(SK_ColorCYAN);
  border_paint.setStrokeWidth(2.0);
  canvas->drawRect(ToSkRect(scroll_extents), border_paint);

  const int kAxisLength = 25;
  const int kAxisThickness = 1;

  // Draw axis as rects instead of lines so when the canvas is scaled, the axis
  // scale relative to the origin, rather than by their stroke center.
  SkPaint x_axis_paint;
  x_axis_paint.setStyle(SkPaint::kFill_Style);
  x_axis_paint.setColor(SK_ColorRED);
  canvas->drawRect(SkRect::MakeXYWH(1, 0, kAxisLength, kAxisThickness),
                   x_axis_paint);

  SkPaint y_axis_paint;
  y_axis_paint.setStyle(SkPaint::kFill_Style);
  y_axis_paint.setColor(SK_ColorGREEN);
  canvas->drawRect(SkRect::MakeXYWH(0, 1, kAxisThickness, kAxisLength),
                   y_axis_paint);

  // Draw an additional diagonal line to help identify the origin if a subframe
  // is scrolled.
  SkPaint diagonal_line_paint;
  border_paint.setStyle(SkPaint::kStroke_Style);
  border_paint.setColor(SK_ColorBLUE);
  border_paint.setStrokeWidth(2.0);
  canvas->drawLine(0, 0, kAxisLength, kAxisLength, diagonal_line_paint);

  canvas->restore();
}

// Fill |frame| and |expected_data| with the remaining parameters and draw and
// save a recording to |path|.
void PopulateFrameProto(
    PaintPreviewFrameProto* frame_proto,
    const base::UnguessableToken& guid,
    bool set_is_main_frame,
    const base::FilePath& path,
    const gfx::Size& scroll_extents,
    std::vector<std::pair<base::UnguessableToken, gfx::RectF>> subframes,
    base::flat_map<base::UnguessableToken, mojom::FrameDataPtr>* expected_data,
    gfx::Size scroll_offsets = gfx::Size(),
    SkColor picture_fill_color = SK_ColorDKGRAY) {
  frame_proto->set_embedding_token_low(guid.GetLowForSerialization());
  frame_proto->set_embedding_token_high(guid.GetHighForSerialization());
  frame_proto->set_is_main_frame(set_is_main_frame);
  frame_proto->set_file_path(path.AsUTF8Unsafe());
  frame_proto->set_scroll_offset_x(scroll_offsets.width());
  frame_proto->set_scroll_offset_y(scroll_offsets.height());

  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(ToSkRect(scroll_extents));
  DrawDummyTestPicture(canvas, picture_fill_color, scroll_extents);

  // It's okay to create a new document guid every time since we're not
  // observing it.
  PaintPreviewTracker tracker(base::UnguessableToken::Create(), guid,
                              set_is_main_frame);
  mojom::FrameDataPtr expected_frame_data = mojom::FrameData::New();
  expected_frame_data->scroll_extents = scroll_extents;
  expected_frame_data->scroll_offsets = scroll_offsets;

  for (const auto& subframe : subframes) {
    const base::UnguessableToken& subframe_id = subframe.first;
    gfx::RectF clip_rect = subframe.second;

    // Record the subframe as custom data to |canvas|.
    uint32_t content_id = tracker.CreateContentForRemoteFrame(
        gfx::ToEnclosingRect(clip_rect), subframe_id);
    tracker.CustomDataToSkPictureCallback(canvas, content_id);

    auto* content_id_embedding_token_pair =
        frame_proto->add_content_id_to_embedding_tokens();
    content_id_embedding_token_pair->set_content_id(content_id);
    content_id_embedding_token_pair->set_embedding_token_low(
        subframe_id.GetLowForSerialization());
    content_id_embedding_token_pair->set_embedding_token_high(
        subframe_id.GetHighForSerialization());
    expected_frame_data->subframes.push_back(
        mojom::SubframeClipRect::New(subframe_id, clip_rect));
    tracker.TransformClipForFrame(content_id);
  }

  sk_sp<SkPicture> pic = recorder.finishRecordingAsPicture();

  size_t serialized_size = 0;
  ASSERT_TRUE(RecordToFile(
      base::File(path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE),
      pic, &tracker, std::nullopt, &serialized_size));
  ASSERT_GE(serialized_size, 0u);

  expected_data->insert({guid, std::move(expected_frame_data)});
}

// Enumeration to select between the |*SeparateFrame*| and |*MainFrame*|
// functions on |PaintPreviewCompositor|.
enum class CompositeType { kSeparateFrame, kMainFrame };

std::string CompositeTypeParamToString(
    const ::testing::TestParamInfo<CompositeType>& composite_type) {
  switch (composite_type.param) {
    case CompositeType::kSeparateFrame:
      return "SeparateFrame";
    case CompositeType::kMainFrame:
      return "MainFrame";
  }
}

}  // namespace

class PaintPreviewCompositorBeginCompositeTest
    : public testing::TestWithParam<CompositeType> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    compositor_.SetRootFrameUrl(url_);
    base::DiscardableMemoryAllocator::SetInstance(&allocator_);
  }

  void TearDown() override {
    base::DiscardableMemoryAllocator::SetInstance(nullptr);
  }

  // Run |Begin*Composite| with |request| and compare the response with
  // the |expected_*| parameters.
  void BeginCompositeAndValidate(
      mojom::PaintPreviewBeginCompositeRequestPtr request,
      mojom::PaintPreviewCompositor::BeginCompositeStatus expected_status,
      const base::UnguessableToken& expected_root_frame_guid,
      base::flat_map<base::UnguessableToken, mojom::FrameDataPtr>
          expected_data) {
    switch (GetParam()) {
      case CompositeType::kSeparateFrame:
        compositor_.BeginSeparatedFrameComposite(
            std::move(request),
            base::BindOnce(&BeginCompositeCallbackImpl, expected_status,
                           expected_root_frame_guid, std::move(expected_data)));
        break;
      case CompositeType::kMainFrame: {
        base::flat_map<base::UnguessableToken, mojom::FrameDataPtr> root_data;
        auto it = expected_data.find(expected_root_frame_guid);
        if (it != expected_data.end()) {
          root_data.insert({expected_root_frame_guid, it->second.Clone()});
          root_data.find(expected_root_frame_guid)->second->subframes.clear();
        }
        compositor_.BeginMainFrameComposite(
            std::move(request),
            base::BindOnce(&BeginCompositeCallbackImpl, expected_status,
                           expected_root_frame_guid, std::move(root_data)));
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  base::ScopedTempDir temp_dir_;

  GURL url_{"https://www.chromium.org"};

 protected:
  base::test::TaskEnvironment task_environment_;

 private:
  base::TestDiscardableMemoryAllocator allocator_;
  PaintPreviewCompositorImpl compositor_{mojo::NullReceiver(), nullptr,
                                         base::DoNothing()};
};

TEST_P(PaintPreviewCompositorBeginCompositeTest, MissingSubFrameRecording) {
  const base::UnguessableToken kRootFrameID = base::UnguessableToken::Create();
  gfx::Size root_frame_scroll_extent(100, 200);
  const base::UnguessableToken kSubframe_0_ID =
      base::UnguessableToken::Create();
  gfx::Size subframe_0_scroll_extent(50, 75);
  gfx::RectF subframe_0_clip_rect(10, 20, 30, 40);
  const base::UnguessableToken kSubframe_0_0_ID =
      base::UnguessableToken::Create();
  gfx::Size subframe_0_0_scroll_extent(20, 20);
  gfx::RectF subframe_0_0_clip_rect(10, 10, 20, 20);
  const base::UnguessableToken kSubframe_0_1_ID =
      base::UnguessableToken::Create();
  gfx::Size subframe_0_1_scroll_extent(10, 5);
  gfx::RectF subframe_0_1_clip_rect(10, 10, 30, 30);
  const base::UnguessableToken kSubframe_1_ID =
      base::UnguessableToken::Create();
  gfx::Size subframe_1_scroll_extent(1, 1);
  gfx::RectF subframe_1_clip_rect(0, 0, 1, 1);

  PaintPreviewProto proto;
  proto.mutable_metadata()->set_url(url_.spec());
  base::flat_map<base::UnguessableToken, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(proto.mutable_root_frame(), kRootFrameID, true,
                     temp_dir_.GetPath().AppendASCII("root.skp"),
                     root_frame_scroll_extent,
                     {{kSubframe_0_ID, subframe_0_clip_rect},
                      {kSubframe_1_ID, subframe_1_clip_rect}},
                     &expected_data);
  PopulateFrameProto(proto.add_subframes(), kSubframe_0_ID, false,
                     temp_dir_.GetPath().AppendASCII("subframe_0.skp"),
                     subframe_0_scroll_extent,
                     {{kSubframe_0_0_ID, subframe_0_0_clip_rect},
                      {kSubframe_0_1_ID, subframe_0_1_clip_rect}},
                     &expected_data);
  PopulateFrameProto(proto.add_subframes(), kSubframe_0_0_ID, false,
                     temp_dir_.GetPath().AppendASCII("subframe_0_0.skp"),
                     subframe_0_0_scroll_extent, {}, &expected_data);
  PopulateFrameProto(proto.add_subframes(), kSubframe_0_1_ID, false,
                     temp_dir_.GetPath().AppendASCII("subframe_0_1.skp"),
                     subframe_0_1_scroll_extent, {}, &expected_data);
  PopulateFrameProto(proto.add_subframes(), kSubframe_1_ID, false,
                     temp_dir_.GetPath().AppendASCII("subframe_1.skp"),
                     subframe_1_scroll_extent, {}, &expected_data);
  auto recording_map = RecordingMapFromPaintPreviewProto(proto);
  // Missing a subframe SKP is still valid. Compositing will ignore it in the
  // results.
  recording_map.erase(kSubframe_0_0_ID);
  expected_data.erase(kSubframe_0_0_ID);
  // Remove the kSubframe_0_0_ID from the subframe list since it isn't
  // available.
  auto& vec = expected_data[kSubframe_0_ID]->subframes;
  vec.front() = std::move(vec.back());
  vec.pop_back();

  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  request->recording_map = std::move(recording_map);
  request->preview = mojo_base::ProtoWrapper(proto);

  BeginCompositeAndValidate(
      std::move(request),
      mojom::PaintPreviewCompositor::BeginCompositeStatus::kPartialSuccess,
      kRootFrameID, std::move(expected_data));
}

// Ensure that depending on a frame multiple times works.
TEST_P(PaintPreviewCompositorBeginCompositeTest, DuplicateFrame) {
  const base::UnguessableToken kRootFrameID = base::UnguessableToken::Create();
  gfx::Size root_frame_scroll_extent(100, 200);
  const base::UnguessableToken kSubframe_0_ID =
      base::UnguessableToken::Create();
  gfx::Size subframe_0_scroll_extent(50, 75);
  gfx::RectF subframe_0_clip_rect(10, 20, 30, 40);

  PaintPreviewProto proto;
  proto.mutable_metadata()->set_url(url_.spec());
  base::flat_map<base::UnguessableToken, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(proto.mutable_root_frame(), kRootFrameID, true,
                     temp_dir_.GetPath().AppendASCII("root.skp"),
                     root_frame_scroll_extent,
                     {{kSubframe_0_ID, subframe_0_clip_rect},
                      {kSubframe_0_ID, subframe_0_clip_rect}},
                     &expected_data);
  PopulateFrameProto(proto.add_subframes(), kSubframe_0_ID, false,
                     temp_dir_.GetPath().AppendASCII("subframe_0.skp"),
                     subframe_0_scroll_extent, {}, &expected_data);

  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  request->recording_map = RecordingMapFromPaintPreviewProto(proto);
  request->preview = mojo_base::ProtoWrapper(proto);

  BeginCompositeAndValidate(
      std::move(request),
      mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess,
      kRootFrameID, std::move(expected_data));
}

// Ensure that a loop in frames works.
TEST_P(PaintPreviewCompositorBeginCompositeTest, FrameDependencyLoop) {
  const base::UnguessableToken kRootFrameID = base::UnguessableToken::Create();
  gfx::Size root_frame_scroll_extent(100, 200);
  const base::UnguessableToken kSubframe_0_ID =
      base::UnguessableToken::Create();
  gfx::Size subframe_0_scroll_extent(50, 75);
  gfx::RectF subframe_0_clip_rect(10, 20, 30, 40);

  PaintPreviewProto proto;
  proto.mutable_metadata()->set_url(url_.spec());
  base::flat_map<base::UnguessableToken, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(proto.mutable_root_frame(), kRootFrameID, true,
                     temp_dir_.GetPath().AppendASCII("root.skp"),
                     root_frame_scroll_extent,
                     {{kSubframe_0_ID, subframe_0_clip_rect}}, &expected_data);
  PopulateFrameProto(proto.add_subframes(), kSubframe_0_ID, false,
                     temp_dir_.GetPath().AppendASCII("subframe_0.skp"),
                     subframe_0_scroll_extent,
                     {{kRootFrameID, subframe_0_clip_rect}}, &expected_data);

  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  request->recording_map = RecordingMapFromPaintPreviewProto(proto);
  request->preview = mojo_base::ProtoWrapper(proto);

  BeginCompositeAndValidate(
      std::move(request),
      GetParam() == CompositeType::kSeparateFrame
          ? mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess
          // In the root frame case, subframes will load until the first cycle.
          : mojom::PaintPreviewCompositor::BeginCompositeStatus::
                kPartialSuccess,
      kRootFrameID, std::move(expected_data));
}

// Ensure that a frame referencing itself works correctly.
TEST_P(PaintPreviewCompositorBeginCompositeTest, SelfReference) {
  const base::UnguessableToken kRootFrameID = base::UnguessableToken::Create();
  gfx::Size root_frame_scroll_extent(100, 200);
  gfx::RectF root_frame_clip_rect(10, 20, 30, 40);

  PaintPreviewProto proto;
  proto.mutable_metadata()->set_url(url_.spec());
  base::flat_map<base::UnguessableToken, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(proto.mutable_root_frame(), kRootFrameID, true,
                     temp_dir_.GetPath().AppendASCII("root.skp"),
                     root_frame_scroll_extent,
                     {{kRootFrameID, root_frame_clip_rect}}, &expected_data);

  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  request->recording_map = RecordingMapFromPaintPreviewProto(proto);
  request->preview = mojo_base::ProtoWrapper(proto);

  BeginCompositeAndValidate(
      std::move(request),
      GetParam() == CompositeType::kSeparateFrame
          ? mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess
          // In the root frame case, a frame cannot be embedded in itself.
          : mojom::PaintPreviewCompositor::BeginCompositeStatus::
                kPartialSuccess,
      kRootFrameID, std::move(expected_data));
}

TEST_P(PaintPreviewCompositorBeginCompositeTest, InvalidRegionHandling) {
  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();

  BeginCompositeAndValidate(
      std::move(request),
      mojom::PaintPreviewCompositor::BeginCompositeStatus::
          kDeserializingFailure,
      base::UnguessableToken::Create(),
      base::flat_map<base::UnguessableToken, mojom::FrameDataPtr>());
}

TEST_P(PaintPreviewCompositorBeginCompositeTest, InvalidProto) {
  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  std::string test_data = "hello world";

  // These calls log errors without a newline (from the proto lib). As a
  // result, the Android gtest parser fails to parse the test status. To work
  // around this gobble the log message.
  {
    testing::internal::CaptureStdout();
    request->preview = mojo_base::ProtoWrapper(
        base::make_span(reinterpret_cast<const uint8_t*>(test_data.c_str()),
                        test_data.size()),
        "paint_preview.PaintPreviewProto",
        mojo_base::ProtoWrapperBytes::GetPassKey());
    BeginCompositeAndValidate(
        std::move(request),
        mojom::PaintPreviewCompositor::BeginCompositeStatus::
            kDeserializingFailure,
        base::UnguessableToken::Create(),
        base::flat_map<base::UnguessableToken, mojom::FrameDataPtr>());
    LOG(ERROR) << testing::internal::GetCapturedStdout();
  }
}

TEST_P(PaintPreviewCompositorBeginCompositeTest, InvalidRootFrame) {
  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  const base::UnguessableToken kRootFrameID = base::UnguessableToken::Create();
  PaintPreviewProto proto;
  proto.mutable_metadata()->set_url(url_.spec());
  base::flat_map<base::UnguessableToken, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(proto.mutable_root_frame(), kRootFrameID, true,
                     temp_dir_.GetPath().AppendASCII("root.skp"),
                     gfx::Size(1, 1), {}, &expected_data);
  auto recording_map = RecordingMapFromPaintPreviewProto(proto);
  recording_map.erase(
      kRootFrameID);  // Missing a SKP for the root file is invalid.
  request->recording_map = std::move(recording_map);
  request->preview = mojo_base::ProtoWrapper(proto);

  BeginCompositeAndValidate(
      std::move(request),
      mojom::PaintPreviewCompositor::BeginCompositeStatus::kCompositingFailure,
      base::UnguessableToken::Create(),
      base::flat_map<base::UnguessableToken, mojom::FrameDataPtr>());
}

// Ensure that scroll offsets are correctly returned in the
// |BeginSeparatedFrameComposite| case.
TEST_P(PaintPreviewCompositorBeginCompositeTest, SubframeWithScrollOffsets) {
  const base::UnguessableToken kRootFrameID = base::UnguessableToken::Create();
  gfx::Size root_frame_scroll_extent(100, 200);
  const base::UnguessableToken kSubframe_0_ID =
      base::UnguessableToken::Create();
  gfx::Size subframe_0_scroll_extent(50, 75);
  gfx::RectF subframe_0_clip_rect(10, 20, 30, 40);
  gfx::Size subframe_0_scroll_offsets(34, 56);

  PaintPreviewProto proto;
  proto.mutable_metadata()->set_url(url_.spec());
  base::flat_map<base::UnguessableToken, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(proto.mutable_root_frame(), kRootFrameID, true,
                     temp_dir_.GetPath().AppendASCII("root.skp"),
                     root_frame_scroll_extent,
                     {{kSubframe_0_ID, subframe_0_clip_rect}}, &expected_data);
  PopulateFrameProto(proto.add_subframes(), kSubframe_0_ID, false,
                     temp_dir_.GetPath().AppendASCII("subframe_0.skp"),
                     subframe_0_scroll_extent, {}, &expected_data,
                     subframe_0_scroll_offsets);

  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  request->recording_map = RecordingMapFromPaintPreviewProto(proto);
  request->preview = mojo_base::ProtoWrapper(proto);

  BeginCompositeAndValidate(
      std::move(request),
      mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess,
      kRootFrameID, std::move(expected_data));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PaintPreviewCompositorBeginCompositeTest,
                         testing::Values(CompositeType::kSeparateFrame,
                                         CompositeType::kMainFrame),
                         CompositeTypeParamToString);

class PaintPreviewCompositorTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::DiscardableMemoryAllocator::SetInstance(&allocator_);
  }

  void TearDown() override {
    base::DiscardableMemoryAllocator::SetInstance(nullptr);
  }

  base::ScopedTempDir temp_dir_;

 protected:
  base::test::TaskEnvironment task_environment_;
  PaintPreviewCompositorImpl compositor_{mojo::NullReceiver(), nullptr,
                                         base::BindOnce([]() {})};

 private:
  base::TestDiscardableMemoryAllocator allocator_;
};

TEST_F(PaintPreviewCompositorTest, TestComposite) {
  GURL url("https://www.chromium.org");
  const base::UnguessableToken kRootFrameID = base::UnguessableToken::Create();
  gfx::Size root_frame_scroll_extent(100, 200);
  PaintPreviewProto proto;
  proto.mutable_metadata()->set_url(url.spec());
  base::flat_map<base::UnguessableToken, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(proto.mutable_root_frame(), kRootFrameID, true,
                     temp_dir_.GetPath().AppendASCII("root.skp"),
                     root_frame_scroll_extent, {}, &expected_data);
  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  request->recording_map = RecordingMapFromPaintPreviewProto(proto);
  request->preview = mojo_base::ProtoWrapper(proto);
  compositor_.BeginSeparatedFrameComposite(
      std::move(request),
      base::BindOnce(
          &BeginCompositeCallbackImpl,
          mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess,
          kRootFrameID, std::move(expected_data)));
  float scale_factor = 2;
  gfx::Rect rect = gfx::ScaleToEnclosingRect(
      gfx::Rect(root_frame_scroll_extent), scale_factor);
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(rect.width(), rect.height()));
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.scale(scale_factor, scale_factor);
  DrawDummyTestPicture(&canvas, SK_ColorDKGRAY, root_frame_scroll_extent);
  compositor_.BitmapForSeparatedFrame(
      kRootFrameID, rect, scale_factor,
      base::BindOnce(&BitmapCallbackImpl,
                     mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                     bitmap));
  task_environment_.RunUntilIdle();
  compositor_.BitmapForSeparatedFrame(
      base::UnguessableToken::Create(), rect, scale_factor,
      base::BindOnce(&BitmapCallbackImpl,
                     mojom::PaintPreviewCompositor::BitmapStatus::kMissingFrame,
                     bitmap));
  task_environment_.RunUntilIdle();
}

TEST_F(PaintPreviewCompositorTest, TestCompositeWithMemoryBuffer) {
  GURL url("https://www.chromium.org");
  const base::UnguessableToken kRootFrameID = base::UnguessableToken::Create();
  gfx::Size root_frame_scroll_extent(100, 200);
  PaintPreviewProto proto;
  proto.mutable_metadata()->set_url(url.spec());

  PaintPreviewFrameProto* root_frame = proto.mutable_root_frame();
  root_frame->set_embedding_token_low(kRootFrameID.GetLowForSerialization());
  root_frame->set_embedding_token_high(kRootFrameID.GetHighForSerialization());
  root_frame->set_is_main_frame(true);

  mojo_base::BigBuffer buffer;
  base::flat_map<base::UnguessableToken, mojom::FrameDataPtr> expected_data;
  {
    SkPictureRecorder recorder;
    SkCanvas* canvas =
        recorder.beginRecording(ToSkRect(root_frame_scroll_extent));
    DrawDummyTestPicture(canvas, SK_ColorDKGRAY, root_frame_scroll_extent);
    sk_sp<SkPicture> pic = recorder.finishRecordingAsPicture();

    PaintPreviewTracker tracker(base::UnguessableToken::Create(), kRootFrameID,
                                /*is_main_frame=*/true);
    size_t serialized_size = 0;
    auto result = RecordToBuffer(pic, &tracker, std::nullopt, &serialized_size);
    ASSERT_TRUE(result.has_value());
    buffer = std::move(result.value());

    mojom::FrameDataPtr frame_data = mojom::FrameData::New();
    frame_data->scroll_extents = root_frame_scroll_extent;
    expected_data.insert({kRootFrameID, std::move(frame_data)});
  }

  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  request->recording_map.insert(
      {kRootFrameID, SerializedRecording(std::move(buffer))});
  request->preview = mojo_base::ProtoWrapper(proto);

  compositor_.BeginSeparatedFrameComposite(
      std::move(request),
      base::BindOnce(
          &BeginCompositeCallbackImpl,
          mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess,
          kRootFrameID, std::move(expected_data)));
  float scale_factor = 2;
  gfx::Rect rect = gfx::ScaleToEnclosingRect(
      gfx::Rect(root_frame_scroll_extent), scale_factor);
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(rect.width(), rect.height()));
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.scale(scale_factor, scale_factor);
  DrawDummyTestPicture(&canvas, SK_ColorDKGRAY, root_frame_scroll_extent);
  compositor_.BitmapForSeparatedFrame(
      kRootFrameID, rect, scale_factor,
      base::BindOnce(&BitmapCallbackImpl,
                     mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                     bitmap));
  task_environment_.RunUntilIdle();
  compositor_.BitmapForSeparatedFrame(
      base::UnguessableToken::Create(), rect, scale_factor,
      base::BindOnce(&BitmapCallbackImpl,
                     mojom::PaintPreviewCompositor::BitmapStatus::kMissingFrame,
                     bitmap));
  task_environment_.RunUntilIdle();
}

TEST_F(PaintPreviewCompositorTest, TestCompositeMainFrameNoDependencies) {
  GURL url("https://www.chromium.org");
  const base::UnguessableToken kRootFrameID = base::UnguessableToken::Create();
  gfx::Size root_frame_scroll_extent(100, 200);
  PaintPreviewProto proto;
  proto.mutable_metadata()->set_url(url.spec());
  base::flat_map<base::UnguessableToken, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(proto.mutable_root_frame(), kRootFrameID, true,
                     temp_dir_.GetPath().AppendASCII("root.skp"),
                     root_frame_scroll_extent, {}, &expected_data);
  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  request->recording_map = RecordingMapFromPaintPreviewProto(proto);
  request->preview = mojo_base::ProtoWrapper(proto);
  compositor_.BeginMainFrameComposite(
      std::move(request),
      base::BindOnce(
          &BeginCompositeCallbackImpl,
          mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess,
          kRootFrameID, std::move(expected_data)));
  float scale_factor = 2;
  gfx::Rect rect = gfx::ScaleToEnclosingRect(
      gfx::Rect(root_frame_scroll_extent), scale_factor);
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(rect.width(), rect.height()));
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.scale(scale_factor, scale_factor);
  DrawDummyTestPicture(&canvas, SK_ColorDKGRAY, root_frame_scroll_extent);
  compositor_.BitmapForMainFrame(
      rect, scale_factor,
      base::BindOnce(&BitmapCallbackImpl,
                     mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                     bitmap));
  task_environment_.RunUntilIdle();
}

TEST_F(PaintPreviewCompositorTest, TestCompositeMainFrameOneDependency) {
  GURL url("https://www.chromium.org");
  const base::UnguessableToken kRootFrameID = base::UnguessableToken::Create();
  gfx::Size root_frame_scroll_extent(100, 200);
  const base::UnguessableToken kSubframe_0_ID =
      base::UnguessableToken::Create();
  gfx::Size subframe_0_scroll_extent(50, 75);
  gfx::RectF subframe_0_clip_rect(10, 20, 30, 40);

  PaintPreviewProto proto;
  proto.mutable_metadata()->set_url(url.spec());
  base::flat_map<base::UnguessableToken, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(proto.mutable_root_frame(), kRootFrameID, true,
                     temp_dir_.GetPath().AppendASCII("root.skp"),
                     root_frame_scroll_extent,
                     {{kSubframe_0_ID, subframe_0_clip_rect}}, &expected_data);
  PopulateFrameProto(proto.add_subframes(), kSubframe_0_ID, false,
                     temp_dir_.GetPath().AppendASCII("subframe_0.skp"),
                     subframe_0_scroll_extent, {}, &expected_data, gfx::Size(),
                     SK_ColorLTGRAY);

  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  request->recording_map = RecordingMapFromPaintPreviewProto(proto);
  request->preview = mojo_base::ProtoWrapper(proto);
  expected_data.erase(kSubframe_0_ID);
  expected_data.find(kRootFrameID)->second->subframes.clear();
  compositor_.BeginMainFrameComposite(
      std::move(request),
      base::BindOnce(
          &BeginCompositeCallbackImpl,
          mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess,
          kRootFrameID, std::move(expected_data)));
  float scale_factor = 1;
  gfx::Rect rect = gfx::ScaleToEnclosingRect(
      gfx::Rect(root_frame_scroll_extent), scale_factor);
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(rect.width(), rect.height()));
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.scale(scale_factor, scale_factor);
  DrawDummyTestPicture(&canvas, SK_ColorDKGRAY, root_frame_scroll_extent);
  // Draw the subframe where we embedded it while populating the proto.
  DrawDummyTestPicture(&canvas, SK_ColorLTGRAY, subframe_0_scroll_extent,
                       subframe_0_clip_rect);
  compositor_.BitmapForMainFrame(
      rect, scale_factor,
      base::BindOnce(&BitmapCallbackImpl,
                     mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                     bitmap));
  task_environment_.RunUntilIdle();
}

TEST_F(PaintPreviewCompositorTest,
       TestCompositeMainFrameOneDependencyScrolled) {
  GURL url("https://www.chromium.org");
  const base::UnguessableToken kRootFrameID = base::UnguessableToken::Create();
  gfx::Size root_frame_scroll_extent(100, 200);
  const base::UnguessableToken kSubframe_0_ID =
      base::UnguessableToken::Create();
  gfx::Size subframe_0_scroll_extent(50, 75);
  gfx::RectF subframe_0_clip_rect(10, 20, 30, 40);
  gfx::Size subframe_0_scroll_offsets(0, 5);

  PaintPreviewProto proto;
  proto.mutable_metadata()->set_url(url.spec());
  base::flat_map<base::UnguessableToken, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(proto.mutable_root_frame(), kRootFrameID, true,
                     temp_dir_.GetPath().AppendASCII("root.skp"),
                     root_frame_scroll_extent,
                     {{kSubframe_0_ID, subframe_0_clip_rect}}, &expected_data);
  PopulateFrameProto(proto.add_subframes(), kSubframe_0_ID, false,
                     temp_dir_.GetPath().AppendASCII("subframe_0.skp"),
                     subframe_0_scroll_extent, {}, &expected_data,
                     subframe_0_scroll_offsets, SK_ColorLTGRAY);

  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  request->recording_map = RecordingMapFromPaintPreviewProto(proto);
  request->preview = mojo_base::ProtoWrapper(proto);
  expected_data.erase(kSubframe_0_ID);
  expected_data.find(kRootFrameID)->second->subframes.clear();
  compositor_.BeginMainFrameComposite(
      std::move(request),
      base::BindOnce(
          &BeginCompositeCallbackImpl,
          mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess,
          kRootFrameID, std::move(expected_data)));
  float scale_factor = 1;
  gfx::Rect rect = gfx::ScaleToEnclosingRect(
      gfx::Rect(root_frame_scroll_extent), scale_factor);
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(rect.width(), rect.height()));
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.scale(scale_factor, scale_factor);
  DrawDummyTestPicture(&canvas, SK_ColorDKGRAY, root_frame_scroll_extent);
  // Draw the subframe where we embedded it while populating the proto.
  DrawDummyTestPicture(&canvas, SK_ColorLTGRAY, subframe_0_scroll_extent,
                       subframe_0_clip_rect, subframe_0_scroll_offsets);
  compositor_.BitmapForMainFrame(
      rect, scale_factor,
      base::BindOnce(&BitmapCallbackImpl,
                     mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                     bitmap));
  task_environment_.RunUntilIdle();
}

TEST_F(PaintPreviewCompositorTest,
       TestCompositeMainFrameOneDependencyWithRootFrameScrolled) {
  GURL url("https://www.chromium.org");
  const base::UnguessableToken kRootFrameID = base::UnguessableToken::Create();
  gfx::Size root_frame_scroll_extent(110, 215);
  gfx::Size root_frame_scroll_offsets(10, 15);
  gfx::RectF root_frame_clip_rect(10, 15, 100, 200);
  const base::UnguessableToken kSubframe_0_ID =
      base::UnguessableToken::Create();
  gfx::Size subframe_0_scroll_extent(50, 75);
  gfx::RectF subframe_0_clip_rect(10, 20, 30, 40);

  PaintPreviewProto proto;
  proto.mutable_metadata()->set_url(url.spec());
  base::flat_map<base::UnguessableToken, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(proto.mutable_root_frame(), kRootFrameID, true,
                     temp_dir_.GetPath().AppendASCII("root.skp"),
                     root_frame_scroll_extent,
                     {{kSubframe_0_ID, subframe_0_clip_rect}}, &expected_data,
                     root_frame_scroll_offsets);
  PopulateFrameProto(proto.add_subframes(), kSubframe_0_ID, false,
                     temp_dir_.GetPath().AppendASCII("subframe_0.skp"),
                     subframe_0_scroll_extent, {}, &expected_data, gfx::Size(),
                     SK_ColorLTGRAY);

  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  request->recording_map = RecordingMapFromPaintPreviewProto(proto);
  request->preview = mojo_base::ProtoWrapper(proto);
  expected_data.erase(kSubframe_0_ID);
  expected_data.find(kRootFrameID)->second->subframes.clear();
  compositor_.BeginMainFrameComposite(
      std::move(request),
      base::BindOnce(
          &BeginCompositeCallbackImpl,
          mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess,
          kRootFrameID, std::move(expected_data)));
  float scale_factor = 1;
  gfx::RectF rect = root_frame_clip_rect;
  rect.Scale(scale_factor);
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(rect.width(), rect.height()));
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.scale(scale_factor, scale_factor);
  // Offset the canvas to simulate the root frame being scrolled.
  canvas.translate(-root_frame_clip_rect.x(), -root_frame_clip_rect.y());
  DrawDummyTestPicture(&canvas, SK_ColorDKGRAY, root_frame_scroll_extent,
                       root_frame_clip_rect, root_frame_scroll_offsets);
  DrawDummyTestPicture(&canvas, SK_ColorLTGRAY, subframe_0_scroll_extent,
                       subframe_0_clip_rect);
  compositor_.BitmapForMainFrame(
      gfx::ToEnclosingRect(rect), scale_factor,
      base::BindOnce(&BitmapCallbackImpl,
                     mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                     bitmap));
  task_environment_.RunUntilIdle();
}

TEST_F(PaintPreviewCompositorTest,
       TestCompositeMainFrameOneDependencyWithRootFrameScrolledWithClamp) {
  GURL url("https://www.chromium.org");
  const base::UnguessableToken kRootFrameID = base::UnguessableToken::Create();
  gfx::Size root_frame_scroll_extent(110, 215);
  gfx::Size root_frame_scroll_offsets(50, 20);
  gfx::RectF root_frame_clip_rect(50, 20, 100, 200);
  const base::UnguessableToken kSubframe_0_ID =
      base::UnguessableToken::Create();
  gfx::Size subframe_0_scroll_extent(50, 75);
  gfx::RectF subframe_0_clip_rect(10, 20, 30, 40);

  PaintPreviewProto proto;
  proto.mutable_metadata()->set_url(url.spec());
  base::flat_map<base::UnguessableToken, mojom::FrameDataPtr> expected_data;
  PopulateFrameProto(proto.mutable_root_frame(), kRootFrameID, true,
                     temp_dir_.GetPath().AppendASCII("root.skp"),
                     root_frame_scroll_extent,
                     {{kSubframe_0_ID, subframe_0_clip_rect}}, &expected_data,
                     root_frame_scroll_offsets);
  PopulateFrameProto(proto.add_subframes(), kSubframe_0_ID, false,
                     temp_dir_.GetPath().AppendASCII("subframe_0.skp"),
                     subframe_0_scroll_extent, {}, &expected_data, gfx::Size(),
                     SK_ColorLTGRAY);

  mojom::PaintPreviewBeginCompositeRequestPtr request =
      mojom::PaintPreviewBeginCompositeRequest::New();
  request->recording_map = RecordingMapFromPaintPreviewProto(proto);
  request->preview = mojo_base::ProtoWrapper(proto);
  expected_data.erase(kSubframe_0_ID);
  expected_data.find(kRootFrameID)->second->subframes.clear();
  compositor_.BeginMainFrameComposite(
      std::move(request),
      base::BindOnce(
          &BeginCompositeCallbackImpl,
          mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess,
          kRootFrameID, std::move(expected_data)));
  float scale_factor = 1;
  root_frame_clip_rect.set_width(110 - 50);
  root_frame_clip_rect.set_height(215 - 20);
  gfx::RectF rect = root_frame_clip_rect;
  rect.Scale(scale_factor);
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(rect.width(), rect.height()));
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.scale(scale_factor, scale_factor);
  // Offset the canvas to simulate the root frame being scrolled.
  canvas.translate(-root_frame_clip_rect.x(), -root_frame_clip_rect.y());
  DrawDummyTestPicture(&canvas, SK_ColorDKGRAY, root_frame_scroll_extent,
                       root_frame_clip_rect, root_frame_scroll_offsets);
  DrawDummyTestPicture(&canvas, SK_ColorLTGRAY, subframe_0_scroll_extent,
                       subframe_0_clip_rect);
  compositor_.BitmapForMainFrame(
      gfx::ToEnclosingRect(rect), scale_factor,
      base::BindOnce(&BitmapCallbackImpl,
                     mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                     bitmap));
  task_environment_.RunUntilIdle();
}

class NoOpDiscardableAllocator : public base::DiscardableMemoryAllocator {
 public:
  NoOpDiscardableAllocator() = default;
  ~NoOpDiscardableAllocator() override = default;

  std::unique_ptr<base::DiscardableMemory> AllocateLockedDiscardableMemory(
      size_t size) override {
    return nullptr;
  }
  size_t GetBytesAllocated() const override { return 0U; }
  void ReleaseFreeMemory() override {}
};

}  // namespace paint_preview
