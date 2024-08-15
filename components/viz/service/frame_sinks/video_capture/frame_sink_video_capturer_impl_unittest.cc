// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_impl.h"

#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/notimplemented.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/token.h"
#include "cc/base/math_util.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/common/surfaces/video_capture_target.h"
#include "components/viz/service/frame_sinks/gmb_video_frame_pool_context_provider.h"
#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_manager.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "media/base/format_utils.h"
#include "media/base/limits.h"
#include "media/base/test_helpers.h"
#include "media/base/video_util.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/video/fake_gpu_memory_buffer.h"
#include "media/video/renderable_gpu_memory_buffer_video_frame_pool.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

using media::VideoCaptureOracle;
using media::VideoFrame;
using media::VideoFrameMetadata;

using testing::_;
using testing::InvokeWithoutArgs;
using testing::NiceMock;
using testing::Return;

namespace viz {
namespace {

bool AlignsWithI420SubsamplingBoundaries(const gfx::Rect& update_rect) {
  return (update_rect.x() % 2 == 0) && (update_rect.y() % 2 == 0) &&
         (update_rect.width() % 2 == 0) && (update_rect.height() % 2 == 0);
}

// Returns true if |frame|'s device scale factor, page scale factor and root
// scroll offset are equal to the expected values.
bool CompareVarsInCompositorFrameMetadata(
    const VideoFrame& frame,
    float device_scale_factor,
    float page_scale_factor,
    const gfx::PointF& root_scroll_offset) {
  auto dsf = frame.metadata().device_scale_factor;
  auto psf = frame.metadata().page_scale_factor;
  auto rso_x = frame.metadata().root_scroll_offset_x;
  auto rso_y = frame.metadata().root_scroll_offset_y;

  bool valid = dsf.has_value() && psf.has_value() && rso_x.has_value() &&
               rso_y.has_value();

  return valid && *dsf == device_scale_factor && *psf == page_scale_factor &&
         gfx::PointF(*rso_x, *rso_y) == root_scroll_offset;
}

// The following functions, CopyOutputRequestFormatToVideoPixelFormat and
// GetColorSpaceForPixelFormat only deal with pixel_format_ which is the user
// requested format, so we only have to care media::PIXEL_FORMAT_ARGB, and
// ResultFormat::RGBA. GetBufferFormatForVideoPixelFormat and
// GetBufferSizeInPixelsForVideoPixelFormat needs to deal with the GMB and frame
// result passed from the capture callback, it could be RGBA/BGRA depends on
// which platform we are, so we have to handle both media::PIXEL_FORMAT_ARGB
// and media::PIXEL_FORMAT_ABGR.

media::VideoPixelFormat CopyOutputRequestFormatToVideoPixelFormat(
    CopyOutputRequest::ResultFormat format) {
  switch (format) {
    case CopyOutputRequest::ResultFormat::I420_PLANES:
      return media::PIXEL_FORMAT_I420;
    case CopyOutputRequest::ResultFormat::NV12:
      return media::PIXEL_FORMAT_NV12;
    case CopyOutputRequest::ResultFormat::RGBA:
      return media::PIXEL_FORMAT_ARGB;
    default:
      NOTREACHED();
  }
}

gfx::ColorSpace GetColorSpaceForPixelFormat(media::VideoPixelFormat format) {
  switch (format) {
    case media::PIXEL_FORMAT_I420:
    case media::PIXEL_FORMAT_NV12:
      return gfx::ColorSpace::CreateREC709();
    case media::PIXEL_FORMAT_ARGB:
      return gfx::ColorSpace::CreateSRGB();
    default:
      NOTREACHED();
  }
}

gfx::Size GetBufferSizeInPixelsForVideoPixelFormat(
    media::VideoPixelFormat format,
    const gfx::Size& coded_size) {
  switch (format) {
    case media::PIXEL_FORMAT_ABGR:
    case media::PIXEL_FORMAT_ARGB:
      return coded_size;
    case media::PIXEL_FORMAT_NV12:
      return {cc::MathUtil::CheckedRoundUp(coded_size.width(), 2),
              cc::MathUtil::CheckedRoundUp(coded_size.height(), 2)};
    default:
      NOTREACHED();
  }
}

// Dummy frame sink ID.
const VideoCaptureTarget kVideoCaptureTarget(FrameSinkId(1, 1));

// The compositor frame interval.
constexpr auto kVsyncInterval = base::Seconds(1) / 60;

const struct SizeSet {
  // The location of the letterboxed content within each VideoFrame. All pixels
  // outside of this region should be black.
  // The expected content rect varies if the format changes. So we dynamically
  // calculate the rect with format.
  gfx::Rect ExpectedContentRect(media::VideoPixelFormat format) const {
    return FrameSinkVideoCapturerImpl::GetContentRectangle(
        gfx::Rect(capture_size), source_size, format);
  }

  gfx::Rect ExpectedContentRect(CopyOutputRequest::ResultFormat format) const {
    return ExpectedContentRect(
        CopyOutputRequestFormatToVideoPixelFormat(format));
  }

  // The source size of the compositor frame sink's Surface.
  gfx::Size source_size;

  // The size of the VideoFrames produced by the capturer.
  gfx::Size capture_size;
} kSizeSets[5] = {{gfx::Size(100, 100), gfx::Size(32, 18)},
                  {gfx::Size(64, 18), gfx::Size(32, 18)},
                  {gfx::Size(64, 18), gfx::Size(64, 18)},
                  {gfx::Size(100, 100), gfx::Size(16, 8)},
                  {gfx::Size(640, 478), gfx::Size(16, 16)}};

constexpr float kDefaultDeviceScaleFactor = 1.f;
constexpr float kDefaultPageScaleFactor = 1.f;
constexpr gfx::PointF kDefaultRootScrollOffset = gfx::PointF(0, 0);

struct YUVColor {
  uint8_t y;
  uint8_t u;
  uint8_t v;
};

YUVColor RGBToYUV(uint32_t argb) {
  auto yuv = media::RGBToYUV(argb);
  return {std::get<0>(yuv), std::get<1>(yuv), std::get<2>(yuv)};
}

// Forces any pending Mojo method calls between the capturer and consumer to be
// made.
void PropagateMojoTasks(
    scoped_refptr<base::TestMockTimeTaskRunner> runner = nullptr) {
  if (runner) {
    runner->RunUntilIdle();
  }
  base::RunLoop().RunUntilIdle();
}

class MockFrameSinkManager : public FrameSinkVideoCapturerManager {
 public:
  MOCK_METHOD1(FindCapturableFrameSink,
               CapturableFrameSink*(const VideoCaptureTarget& target));
  MOCK_METHOD1(OnCapturerConnectionLost,
               void(FrameSinkVideoCapturerImpl* capturer));
};

class MockConsumer : public mojom::FrameSinkVideoConsumer {
 public:
  MockConsumer() {}

  MOCK_METHOD0(OnFrameCapturedMock, void());
  MOCK_METHOD1(OnNewSubCaptureTargetVersion, void(uint32_t));
  MOCK_METHOD0(OnStopped, void());
  MOCK_METHOD1(OnLog, void(const std::string&));

  int num_frames_received() const { return frames_.size(); }

  scoped_refptr<VideoFrame> TakeFrame(int i) { return std::move(frames_[i]); }

  void SendDoneNotification(int i) {
    std::move(done_callbacks_[i]).Run();
    PropagateMojoTasks();
  }

  mojo::PendingRemote<mojom::FrameSinkVideoConsumer> BindVideoConsumer() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  int num_frames_with_empty_region() const {
    return num_frames_with_empty_region_;
  }

 private:
  void OnFrameCaptured(
      media::mojom::VideoBufferHandlePtr data,
      media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& expected_content_rect,
      mojo::PendingRemote<mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks) final {
    CHECK(info);

    mojo::Remote callbacks_remote(std::move(callbacks));
    scoped_refptr<media::VideoFrame> frame;

    if (data->is_read_only_shmem_region()) {
      // kDefault + I420 / RGBA
      base::ReadOnlySharedMemoryRegion& shmem_region =
          data->get_read_only_shmem_region();

      // The |data| parameter is not nullable and mojo type mapping for
      // `base::ReadOnlySharedMemoryRegion` defines that nullable version of it
      // is the same type, with null check being equivalent to IsValid() check.
      // Given the above, we should never be able to receive a read only shmem
      // region that is not valid - mojo will enforce it for us.
      CHECK(shmem_region.IsValid());

      auto required_bytes_to_hold_planes = media::VideoFrame::AllocationSize(
          info->pixel_format, info->coded_size);
      ASSERT_LE(required_bytes_to_hold_planes, shmem_region.GetSize());

      // Map the shared memory buffer and re-constitute a VideoFrame instance
      // around it for analysis via TakeFrame().
      base::ReadOnlySharedMemoryMapping mapping = shmem_region.Map();
      ASSERT_TRUE(mapping.IsValid());
      ASSERT_LE(required_bytes_to_hold_planes, mapping.size());
      frame = media::VideoFrame::WrapExternalData(
          info->pixel_format, info->coded_size, info->visible_rect,
          info->visible_rect.size(), mapping.GetMemoryAs<const uint8_t>(),
          mapping.size(), info->timestamp);
      ASSERT_TRUE(frame);
      frame->AddDestructionObserver(
          base::BindOnce([](base::ReadOnlySharedMemoryMapping mapping) {},
                         std::move(mapping)));
    } else if (data->is_gpu_memory_buffer_handle()) {
      // kNativeTexture + NV12 / RGBA
      // Create a fake GpuMemoryBuffer as these test don't run the code to
      // produce GPU frames. The mailbox values aren't important since
      // IsLetterboxedFrame does no verification for GMB VideoFrames.
      auto gmb_dummy = std::make_unique<media::FakeGpuMemoryBuffer>(
          GetBufferSizeInPixelsForVideoPixelFormat(info->pixel_format,
                                                   info->coded_size),
          VideoPixelFormatToGfxBufferFormat(info->pixel_format).value());

      // The frame is only gonna tell Letterbox to skip the test.
      frame = media::VideoFrame::WrapExternalGpuMemoryBuffer(
          info->visible_rect, info->visible_rect.size(), std::move(gmb_dummy),
          info->timestamp);
      ASSERT_TRUE(frame);
    } else {
      NOTREACHED();
    }

    frame->set_metadata(info->metadata);
    frame->set_color_space(info->color_space);
    OnFrameCapturedMock();
    frames_.push_back(std::move(frame));
    done_callbacks_.push_back(
        base::BindOnce(&mojom::FrameSinkVideoConsumerFrameCallbacks::Done,
                       std::move(callbacks_remote)));
  }

  void OnFrameWithEmptyRegionCapture() final {
    ++num_frames_with_empty_region_;
  }

  int num_frames_with_empty_region_ = 0;

  mojo::Receiver<mojom::FrameSinkVideoConsumer> receiver_{this};
  std::vector<scoped_refptr<VideoFrame>> frames_;
  std::vector<base::OnceClosure> done_callbacks_;
};

class FakeGpuCopyResult : public CopyOutputResult {
 public:
  FakeGpuCopyResult(Format format, const gfx::Rect rect)
      : CopyOutputResult(format,
                         CopyOutputResult::Destination::kNativeTextures,
                         rect,
                         false),
        format_(format),
        result_(TextureResult(
            gpu::Mailbox{},
            GetColorSpaceForPixelFormat(
                CopyOutputRequestFormatToVideoPixelFormat(format)))) {}

  const TextureResult* GetTextureResult() const final { return &result_; }

 private:
  Format format_;
  TextureResult result_;
};

class SolidColorRGBAResult : public CopyOutputResult {
 public:
  SolidColorRGBAResult(const gfx::Rect rect, SkColor color)
      : CopyOutputResult(CopyOutputResult::Format::RGBA,
                         CopyOutputResult::Destination::kSystemMemory,
                         rect,
                         false) {
    bitmap_.setInfo(SkImageInfo::MakeN32Premul(size().width(), size().height(),
                                               SkColorSpace::MakeSRGB()));
    bitmap_.allocPixels();
    bitmap_.eraseColor(color);
  }

  // Instead of manually copying pixels to the buffer, we can use the existing
  // default implementation of CopyOutputRequest, and just pass the prepared
  // bitmap.
  const SkBitmap& AsSkBitmap() const final { return bitmap_; }

 private:
  SkBitmap bitmap_;
};

class SolidColorI420Result : public CopyOutputResult {
 public:
  SolidColorI420Result(const gfx::Rect rect, YUVColor color)
      : CopyOutputResult(CopyOutputResult::Format::I420_PLANES,
                         CopyOutputResult::Destination::kSystemMemory,
                         rect,
                         false),
        color_(color) {}

  bool ReadI420Planes(uint8_t* y_out,
                      int y_out_stride,
                      uint8_t* u_out,
                      int u_out_stride,
                      uint8_t* v_out,
                      int v_out_stride) const final {
    CHECK(y_out);
    CHECK(y_out_stride >= size().width());
    CHECK(u_out);
    const int chroma_width = (size().width() + 1) / 2;
    CHECK(u_out_stride >= chroma_width);
    CHECK(v_out);
    CHECK(v_out_stride >= chroma_width);

    for (int i = 0; i < size().height(); ++i, y_out += y_out_stride) {
      memset(y_out, color_.y, size().width());
    }
    const int chroma_height = (size().height() + 1) / 2;
    for (int i = 0; i < chroma_height; ++i, u_out += u_out_stride) {
      memset(u_out, color_.u, chroma_width);
    }
    for (int i = 0; i < chroma_height; ++i, v_out += v_out_stride) {
      memset(v_out, color_.v, chroma_width);
    }
    return true;
  }

 private:
  const YUVColor color_;
};

class FakeCapturableFrameSink : public CapturableFrameSink {
 public:
  FakeCapturableFrameSink() : size_set_(kSizeSets[0]) {
    metadata_.root_scroll_offset = kDefaultRootScrollOffset;
    metadata_.page_scale_factor = kDefaultPageScaleFactor;
    metadata_.device_scale_factor = kDefaultDeviceScaleFactor;
  }

  Client* attached_client() const { return client_; }

  const FrameSinkId& GetFrameSinkId() const override {
    return kVideoCaptureTarget.frame_sink_id;
  }

  void AttachCaptureClient(Client* client) override {
    ASSERT_FALSE(client_);
    ASSERT_TRUE(client);
    client_ = client;
    if (client_->IsVideoCaptureStarted()) {
      OnClientCaptureStarted();
    }
  }

  void DetachCaptureClient(Client* client) override {
    ASSERT_TRUE(client);
    ASSERT_EQ(client, client_);
    if (client_->IsVideoCaptureStarted()) {
      OnClientCaptureStopped();
    }

    client_ = nullptr;
  }

  std::optional<CapturableFrameSink::RegionProperties>
  GetRequestRegionProperties(
      const VideoCaptureSubTarget& sub_target) const override {
    if (size_set_.source_size.IsEmpty()) {
      return {};
    }

    CapturableFrameSink::RegionProperties out;
    out.root_render_pass_size = size_set_.source_size;
    if (IsEntireTabCapture(sub_target)) {
      out.render_pass_subrect = gfx::Rect(out.root_render_pass_size);
    } else if (IsRegionCapture(sub_target)) {
      current_capture_id_ = SubtreeCaptureId();
      current_crop_id_ = absl::get<RegionCaptureCropId>(sub_target);
      if (current_crop_id_.is_zero() || crop_bounds_.IsEmpty()) {
        return {};
      }

      out.render_pass_subrect = crop_bounds_;
    } else if (IsSubtreeCapture(sub_target)) {
      current_capture_id_ = absl::get<SubtreeCaptureId>(sub_target);
      current_crop_id_ = RegionCaptureCropId();
      if (!current_capture_id_.is_valid() || capture_bounds_.IsEmpty()) {
        return {};
      }

      out.render_pass_subrect = capture_bounds_;
    }

    return out;
  }

  void OnClientCaptureStarted() override { ++number_clients_capturing_; }

  void OnClientCaptureStopped() override { --number_clients_capturing_; }

  void RequestCopyOfOutput(
      PendingCopyOutputRequest pending_copy_output_request) override {
    auto& request = pending_copy_output_request.copy_output_request;
    EXPECT_NE(base::UnguessableToken(), request->source());
    if (pending_copy_output_request.subtree_capture_id.is_valid()) {
      EXPECT_EQ(capture_bounds_, request->area());
    } else {
      EXPECT_TRUE(gfx::Rect(size_set_.source_size).Contains(request->area()));
    }
    auto expected_content_rect = gfx::Rect(
        size_set_.ExpectedContentRect(request->result_format()).size());
    EXPECT_EQ(expected_content_rect, request->result_selection());

    std::unique_ptr<CopyOutputResult> result;
    switch (request->result_destination()) {
      case CopyOutputResult::Destination::kSystemMemory: {
        // We need to construct the source frame.
        switch (request->result_format()) {
          case CopyOutputRequest::ResultFormat::I420_PLANES: {
            result = std::make_unique<SolidColorI420Result>(
                request->result_selection(), RGBToYUV(color_));
            break;
          }
          case CopyOutputRequest::ResultFormat::RGBA: {
            result = std::make_unique<SolidColorRGBAResult>(
                request->result_selection(), color_);
            break;
          }
          default: {
            NOTREACHED();
          }
        }
        break;
      }
      case CopyOutputResult::Destination::kNativeTextures: {
        // We don't need to provide a real GPU result.
        result = std::make_unique<FakeGpuCopyResult>(
            request->result_format(), request->result_selection());
        break;
      }
      default: {
        NOTREACHED();
      }
    }
    results_.push_back(base::BindOnce(
        [](std::unique_ptr<CopyOutputRequest> request,
           std::unique_ptr<CopyOutputResult> result) {
          request->SendResult(std::move(result));
        },
        std::move(request), std::move(result)));
  }

  const CompositorFrameMetadata* GetLastActivatedFrameMetadata() override {
    return &metadata_;
  }

  const gfx::Size& source_size() const { return size_set_.source_size; }

  const RegionCaptureCropId& current_crop_id() const {
    return current_crop_id_;
  }

  const SubtreeCaptureId& current_capture_id() const {
    return current_capture_id_;
  }

  void set_crop_bounds(const gfx::Rect& crop_bounds) {
    crop_bounds_ = crop_bounds;
  }

  void set_capture_bounds(const gfx::Rect& capture_bounds) {
    capture_bounds_ = capture_bounds;
  }

  void set_size_set(const SizeSet& size_set) { size_set_ = size_set; }

  void set_metadata(const CompositorFrameMetadata& metadata) {
    metadata_ = metadata.Clone();
  }

  void SetCopyOutputColor(SkColor color) { color_ = color; }

  // Returns number of copy output result callbacks that have been prepared to
  // be sent back to the capturer. A new result callback is inserted every
  // time a new CopyOutputRequest arrives and does not correspond to the
  // number of results that have actually already been sent. Sending a result
  // is done via |SendCopyOutputResult()|.
  int num_copy_results() const { return results_.size(); }

  void SendCopyOutputResult(int offset) {
    auto it = results_.begin() + offset;
    std::move(*it).Run();
    PropagateMojoTasks(task_runner_);
  }

  void set_task_runner(scoped_refptr<base::TestMockTimeTaskRunner> runner) {
    task_runner_ = std::move(runner);
  }

  int number_clients_capturing() const { return number_clients_capturing_; }

 private:
  // Number of clients that have started capturing.
  int number_clients_capturing_ = 0;
  raw_ptr<CapturableFrameSink::Client> client_ = nullptr;
  // YUV {0xde, 0xad, 0xbf};
  SkColor color_ = SkColorSetARGB(255, 255, 161, 255);
  SizeSet size_set_;
  CompositorFrameMetadata metadata_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  mutable RegionCaptureCropId current_crop_id_;
  mutable SubtreeCaptureId current_capture_id_;
  gfx::Rect crop_bounds_;
  gfx::Rect capture_bounds_;
  std::vector<base::OnceClosure> results_;
};

class InstrumentedVideoCaptureOracle : public media::VideoCaptureOracle {
 public:
  explicit InstrumentedVideoCaptureOracle(bool enable_auto_throttling)
      : media::VideoCaptureOracle(enable_auto_throttling),
        return_false_on_complete_capture_(false) {}

  bool CompleteCapture(int frame_number,
                       bool capture_was_successful,
                       base::TimeTicks* frame_timestamp) override {
    capture_was_successful &= !return_false_on_complete_capture_;
    return media::VideoCaptureOracle::CompleteCapture(
        frame_number, capture_was_successful, frame_timestamp);
  }

  void set_return_false_on_complete_capture(bool should_return_false) {
    return_false_on_complete_capture_ = should_return_false;
  }

  gfx::Size capture_size() const override {
    if (forced_capture_size_.has_value()) {
      return forced_capture_size_.value();
    }
    return media::VideoCaptureOracle::capture_size();
  }

  void set_forced_capture_size(std::optional<gfx::Size> size) {
    forced_capture_size_ = size;
  }

 private:
  bool return_false_on_complete_capture_;
  std::optional<gfx::Size> forced_capture_size_;
};

namespace {
bool IsLetterboxedI420Plane(int plane,
                            uint8_t component,
                            gfx::Rect content_rect,
                            const VideoFrame& frame,
                            testing::MatchResultListener* result_listener) {
  gfx::Rect content_rect_copy = content_rect;
  if (plane != VideoFrame::Plane::kY) {
    content_rect_copy = gfx::Rect(
        content_rect_copy.x() / 2, content_rect_copy.y() / 2,
        content_rect_copy.width() / 2, content_rect_copy.height() / 2);
  }
  for (int row = 0; row < frame.rows(plane); ++row) {
    const uint8_t* p = frame.visible_data(plane) + row * frame.stride(plane);
    for (int col = 0; col < frame.row_bytes(plane); ++col) {
      if (content_rect_copy.Contains(gfx::Point(col, row))) {
        if (p[col] != component) {
          *result_listener << " where pixel at (" << col << ", " << row
                           << ") should be inside content rectangle and the "
                              "component should match 0x"
                           << std::hex << static_cast<unsigned int>(component)
                           << " but is 0x" << std::hex
                           << static_cast<unsigned int>(p[col]);
          return false;
        }
      } else {  // Letterbox border around content.
        if (plane == VideoFrame::Plane::kY && p[col] != 0x00) {
          *result_listener << " where pixel at (" << col << ", " << row
                           << ") should be outside content rectangle and the "
                              "component should match 0x00 but is 0x"
                           << std::hex << static_cast<unsigned int>(p[col]);
          return false;
        }
      }
    }
  }
  return true;
}

bool IsLetterboxedRGBA(SkColor color,
                       gfx::Rect content_rect,
                       const VideoFrame& frame,
                       testing::MatchResultListener* result_listener) {
  SkBitmap bitmap;
  auto bitmap_info = SkImageInfo::MakeN32Premul(frame.coded_size().width(),
                                                frame.coded_size().height());
  bitmap.installPixels(
      bitmap_info,
      const_cast<uint8_t*>(frame.visible_data(VideoFrame::Plane::kARGB)),
      frame.stride(VideoFrame::Plane::kARGB));

  for (int row = 0; row < bitmap.height(); ++row) {
    for (int col = 0; col < bitmap.width(); ++col) {
      SkColor pixel = bitmap.getColor(col, row);
      if (content_rect.Contains(gfx::Point(col, row))) {
        if (pixel != color) {
          *result_listener << " where pixel at (" << col << ", " << row
                           << ") should be inside content rectangle and the "
                              "component should match "
                           << color_utils::SkColorToRgbString(color)
                           << " but is "
                           << color_utils::SkColorToRgbString(pixel);
          return false;
        }
      } else {  // Letterbox border around content.
        constexpr SkColor kLetterboxColor = SK_ColorTRANSPARENT;
        if (pixel != kLetterboxColor) {
          *result_listener << " where pixel at (" << col << ", " << row
                           << ") should be outside content rectangle and the "
                              "component should match "
                           << color_utils::SkColorToRgbString(kLetterboxColor)
                           << " but is "
                           << color_utils::SkColorToRgbString(pixel);
          return false;
        }
      }
    }
  }
  return true;
}
}  // namespace

// Matcher that returns true if the content region of a letterboxed VideoFrame
// is filled with the given color, and black everywhere else.
MATCHER_P3(IsLetterboxedFrame, color, content_rect, pixel_format, "") {
  if (!arg) {
    return false;
  }

  const VideoFrame& frame = *arg;

  // Pretend kUseGpuMemoryBuffer rendered corrected data.
  if (frame.HasMappableGpuBuffer()) {
    return true;
  }

  switch (pixel_format) {
    case media::PIXEL_FORMAT_ARGB: {
      return IsLetterboxedRGBA(color, content_rect, frame, result_listener);
    }
    case media::PIXEL_FORMAT_I420: {
      const YUVColor yuvColor = RGBToYUV(color);
      return IsLetterboxedI420Plane(VideoFrame::Plane::kY, yuvColor.y,
                                    content_rect, frame, result_listener) &&
             IsLetterboxedI420Plane(VideoFrame::Plane::kU, yuvColor.u,
                                    content_rect, frame, result_listener) &&
             IsLetterboxedI420Plane(VideoFrame::Plane::kV, yuvColor.v,
                                    content_rect, frame, result_listener);
    }
    default: {
      NOTREACHED();
    }
  }
}

class TestVideoCaptureOverlay : public VideoCaptureOverlay {
 public:
  using PropertiesCallback =
      base::RepeatingCallback<void(const CapturedFrameProperties&)>;
  TestVideoCaptureOverlay(
      FrameSource& frame_source,
      mojo::PendingReceiver<mojom::FrameSinkVideoCaptureOverlay> receiver,
      PropertiesCallback properties_cb)
      : VideoCaptureOverlay(frame_source, std::move(receiver)),
        properties_cb_(std::move(properties_cb)) {}
  ~TestVideoCaptureOverlay() override = default;

  OnceRenderer MakeRenderer(
      const CapturedFrameProperties& properties) override {
    properties_cb_.Run(properties);
    return {};
  }

 private:
  PropertiesCallback properties_cb_;
};

class TestGmbVideoFramePoolContext
    : public media::RenderableGpuMemoryBufferVideoFramePool::Context {
 public:
  TestGmbVideoFramePoolContext()
      : context_provider_(TestContextProvider::Create()) {}
  ~TestGmbVideoFramePoolContext() override = default;

  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override {
    return std::make_unique<media::FakeGpuMemoryBuffer>(size, format);
  }

  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      gfx::GpuMemoryBuffer* gpu_memory_buffer,
      const SharedImageFormat& si_format,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      gpu::SyncToken& sync_token) override {
    return context_provider_->SharedImageInterface()->CreateSharedImage(
        {si_format, gpu_memory_buffer->GetSize(), color_space, surface_origin,
         alpha_type, usage, "FrameSinkVideoCapturerImplUnittest"},
        gpu_memory_buffer->CloneHandle());
  }

  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      const gfx::Size& size,
      gfx::BufferUsage buffer_usage,
      const SharedImageFormat& si_format,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      gpu::SyncToken& sync_token) override {
    context_provider_->SharedImageInterface()
        ->UseTestGMBInSharedImageCreationWithBufferUsage();
    return context_provider_->SharedImageInterface()->CreateSharedImage(
        {si_format, size, color_space, surface_origin, alpha_type, usage,
         "FrameSinkVideoCapturerImplUnittest"},
        gpu::kNullSurfaceHandle, buffer_usage);
  }

  void DestroySharedImage(const gpu::SyncToken& sync_token,
                          scoped_refptr<gpu::ClientSharedImage> shared_image,
                          const bool is_mappable_si_enabled) override {
    CHECK(shared_image);
    if (is_mappable_si_enabled) {
      shared_image->UpdateDestructionSyncToken(sync_token);
    } else {
      context_provider_->SharedImageInterface()->DestroySharedImage(
          sync_token, std::move(shared_image));
    }
  }

 private:
  scoped_refptr<TestContextProvider> context_provider_;
};

class TestGmbVideoFramePoolContextProvider
    : public GmbVideoFramePoolContextProvider {
 public:
  ~TestGmbVideoFramePoolContextProvider() override = default;

  std::unique_ptr<media::RenderableGpuMemoryBufferVideoFramePool::Context>
  CreateContext(base::OnceClosure on_context_lost) final {
    return std::make_unique<TestGmbVideoFramePoolContext>();
  }
};

}  // namespace

class FrameSinkVideoCapturerTest
    : public testing::Test,
      public testing::WithParamInterface<
          std::tuple<mojom::BufferFormatPreference, media::VideoPixelFormat>> {
 public:
  FrameSinkVideoCapturerTest()
      : size_set_(kSizeSets[0]),
        buffer_format_preference_(std::get<0>(GetParam())),
        pixel_format_(std::get<1>(GetParam())) {
    auto oracle = std::make_unique<InstrumentedVideoCaptureOracle>(
        true /* enable_auto_throttling */);
    oracle_ = oracle.get();

    gmb_context_provider_ =
        std::make_unique<TestGmbVideoFramePoolContextProvider>();

    capturer_ = std::make_unique<FrameSinkVideoCapturerImpl>(
        frame_sink_manager_, gmb_context_provider_.get(), mojo::NullReceiver(),
        std::move(oracle), false);
  }

  void SetUp() override {
    // Override the capturer's TickClock with a virtual clock managed by a
    // manually-driven task runner.
    task_runner_ = new base::TestMockTimeTaskRunner(
        base::Time::Now(), base::TimeTicks() + base::Seconds(1),
        base::TestMockTimeTaskRunner::Type::kStandalone);
    start_time_ = task_runner_->NowTicks();
    capturer_->clock_ = task_runner_->GetMockTickClock();

    // Ensure any posted tasks for CopyOutputResults will be handled when
    // PropagateMojoTasks() is called
    frame_sink_.set_task_runner(task_runner_);

    // Replace the retry timer with one that uses this test's fake clock and
    // task runner.
    capturer_->refresh_frame_retry_timer_.emplace(
        task_runner_->GetMockTickClock());
    capturer_->refresh_frame_retry_timer_->SetTaskRunner(task_runner_);

    // Before setting the format, ensure the defaults are in-place. Then, for
    // these tests, set a specific format and color space.
    ASSERT_EQ(FrameSinkVideoCapturerImpl::kDefaultPixelFormat,
              capturer_->pixel_format_);
    capturer_->SetFormat(pixel_format_);
    ASSERT_EQ(pixel_format_, capturer_->pixel_format_);

    // Set min capture period as small as possible so that the
    // media::VideoCapturerOracle used by the capturer will want to capture
    // every composited frame. The capturer will override the too-small value of
    // zero with a value based on media::limits::kMaxFramesPerSecond.
    capturer_->SetMinCapturePeriod(base::TimeDelta());
    ASSERT_LT(base::TimeDelta(), oracle_->min_capture_period());

    capturer_->SetResolutionConstraints(size_set_.capture_size,
                                        size_set_.capture_size, false);
  }

  void TearDown() override { task_runner_->ClearPendingTasks(); }

  void StartCapture(MockConsumer* consumer) {
    capturer_->Start(consumer->BindVideoConsumer(), buffer_format_preference_);
    PropagateMojoTasks();
  }

  void StopCapture() {
    capturer_->Stop();
    PropagateMojoTasks();
  }

  bool IsUsingGpuMemoryBuffer() {
    return buffer_format_preference_ ==
           mojom::BufferFormatPreference::kPreferGpuMemoryBuffer;
  }

  base::TimeTicks GetNextVsync() const {
    const auto now = task_runner_->NowTicks();
    return now + kVsyncInterval - ((now - start_time_) % kVsyncInterval);
  }

  void AdvanceClockToNextVsync() {
    task_runner_->FastForwardBy(GetNextVsync() - task_runner_->NowTicks());
  }

  void SwitchToSizeSet(const SizeSet& size_set) {
    size_set_ = size_set;
    oracle_->set_forced_capture_size(size_set.capture_size);
    frame_sink_.set_size_set(size_set);
    capturer_->SetResolutionConstraints(size_set_.capture_size,
                                        size_set_.capture_size, false);
  }

  void ForceOracleSize(const SizeSet& size_set) {
    size_set_ = size_set;
    oracle_->set_forced_capture_size(size_set.capture_size);
    frame_sink_.set_size_set(size_set);
    // No call to capturer_, because this method simulates size change requested
    // by the oracle, internal to the capturer.
  }

  const SizeSet& size_set() { return size_set_; }

  void NotifyFrameDamaged(
      gfx::Rect damage_rect,
      float device_scale_factor = kDefaultDeviceScaleFactor,
      float page_scale_factor = kDefaultPageScaleFactor,
      gfx::PointF root_scroll_offset = kDefaultRootScrollOffset) {
    CompositorFrameMetadata metadata;

    metadata.device_scale_factor = device_scale_factor;
    metadata.page_scale_factor = page_scale_factor;
    metadata.root_scroll_offset = root_scroll_offset;

    frame_sink_.set_metadata(metadata);

    capturer_->OnFrameDamaged(frame_sink_.source_size(), damage_rect,
                              GetNextVsync(), metadata);
  }

  void NotifyTargetWentAway() {
    capturer_->OnTargetWillGoAway();
    PropagateMojoTasks();
  }

  bool IsRefreshRetryTimerRunning() {
    return capturer_->refresh_frame_retry_timer_->IsRunning();
  }

  void AdvanceClockForRefreshTimer() {
    task_runner_->FastForwardBy(capturer_->GetDelayBeforeNextRefreshAttempt());
    PropagateMojoTasks();
  }

  gfx::Rect ExpandRectToI420SubsampleBoundaries(const gfx::Rect& rect) {
    return FrameSinkVideoCapturerImpl::ExpandRectToI420SubsampleBoundaries(
        rect);
  }

  void InsertOverlay(std::unique_ptr<VideoCaptureOverlay> overlay) {
    capturer_->overlays_.insert_or_assign(capturer_->overlays_.size() + 1,
                                          std::move(overlay));
  }

 protected:
  SizeSet size_set_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::TimeTicks start_time_;
  MockFrameSinkManager frame_sink_manager_;
  FakeCapturableFrameSink frame_sink_;

  std::unique_ptr<TestGmbVideoFramePoolContextProvider> gmb_context_provider_;
  std::unique_ptr<FrameSinkVideoCapturerImpl> capturer_;

  raw_ptr<InstrumentedVideoCaptureOracle> oracle_;
  mojom::BufferFormatPreference buffer_format_preference_;
  media::VideoPixelFormat pixel_format_;
};

// Tests that the capturer attaches to a frame sink immediately, in the case
// where the frame sink was already known by the manager.
TEST_P(FrameSinkVideoCapturerTest, ResolvesTargetImmediately) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kVideoCaptureTarget))
      .WillRepeatedly(Return(&frame_sink_));

  EXPECT_FALSE(capturer_->target());
  capturer_->ChangeTarget(kVideoCaptureTarget,
                          /*sub_capture_target_version=*/0);
  EXPECT_EQ(kVideoCaptureTarget.frame_sink_id,
            capturer_->target()->frame_sink_id);
  EXPECT_EQ(capturer_.get(), frame_sink_.attached_client());
}

// Tests that the capturer attaches to a frame sink later, in the case where the
// frame sink becomes known to the manager at some later point.
TEST_P(FrameSinkVideoCapturerTest, ResolvesTargetLater) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kVideoCaptureTarget))
      .WillRepeatedly(Return(nullptr));

  EXPECT_FALSE(capturer_->target());
  capturer_->ChangeTarget(kVideoCaptureTarget,
                          /*sub_capture_target_version=*/0);
  EXPECT_EQ(kVideoCaptureTarget.frame_sink_id,
            capturer_->target()->frame_sink_id);
  EXPECT_EQ(nullptr, frame_sink_.attached_client());

  capturer_->SetResolvedTarget(&frame_sink_);
  EXPECT_EQ(capturer_.get(), frame_sink_.attached_client());
}

// Tests that no initial frame is sent after Start() is called until after the
// target has been resolved.
TEST_P(FrameSinkVideoCapturerTest, PostponesCaptureWithoutATarget) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kVideoCaptureTarget))
      .WillRepeatedly(Return(&frame_sink_));

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnFrameCapturedMock()).Times(0);
  EXPECT_CALL(consumer, OnStopped()).Times(1);

  StartCapture(&consumer);
  // No copy requests should have been issued/executed.
  EXPECT_EQ(0, frame_sink_.num_copy_results());
  // The refresh timer is running, which represents the need for an initial
  // frame to be sent.
  EXPECT_TRUE(IsRefreshRetryTimerRunning());

  // Simulate several refresh timer intervals elapsing and the timer firing.
  // Nothing should happen because the capture target was never set.
  for (int i = 0; i < 5; ++i) {
    AdvanceClockForRefreshTimer();
    ASSERT_EQ(0, frame_sink_.num_copy_results());
    ASSERT_TRUE(IsRefreshRetryTimerRunning());
  }

  // Now, set the target. As it resolves, the capturer will immediately attempt
  // a refresh capture, which will cancel the timer and trigger a copy request.
  capturer_->ChangeTarget(kVideoCaptureTarget,
                          /*sub_capture_target_version=*/0);
  EXPECT_EQ(1, frame_sink_.num_copy_results());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  StopCapture();
  EXPECT_FALSE(IsRefreshRetryTimerRunning());
}

// An end-to-end pipeline test where compositor updates trigger the capturer to
// make copy requests, and a stream of video frames is delivered to the
// consumer.
TEST_P(FrameSinkVideoCapturerTest, CapturesCompositedFrames) {
  frame_sink_.SetCopyOutputColor(SkColorSetARGB(255, 128, 128, 128));
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kVideoCaptureTarget))
      .WillRepeatedly(Return(&frame_sink_));

  capturer_->ChangeTarget(kVideoCaptureTarget,
                          /*sub_capture_target_version=*/0);
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  MockConsumer consumer;
  const int num_refresh_frames = 1;
  const int num_update_frames =
      3 * FrameSinkVideoCapturerImpl::kDesignLimitMaxFrames;
  EXPECT_CALL(consumer, OnFrameCapturedMock())
      .Times(num_refresh_frames + num_update_frames);
  EXPECT_CALL(consumer, OnStopped()).Times(1);
  StartCapture(&consumer);

  // Since the target was already resolved at start, the capturer will have
  // immediately executed a refresh capture and triggered a copy request.
  ASSERT_EQ(num_refresh_frames, frame_sink_.num_copy_results());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  // Simulate execution of the copy request and expect to see the initial
  // refresh frame delivered to the consumer.
  frame_sink_.SendCopyOutputResult(0);
  ASSERT_EQ(num_refresh_frames, consumer.num_frames_received());
  EXPECT_THAT(consumer.TakeFrame(0),
              IsLetterboxedFrame(SkColorSetARGB(255, 128, 128, 128),
                                 size_set().ExpectedContentRect(pixel_format_),
                                 pixel_format_));
  consumer.SendDoneNotification(0);

  // Drive the capturer pipeline for a series of frame composites.
  base::TimeDelta last_timestamp;
  for (int i = num_refresh_frames; i < num_refresh_frames + num_update_frames;
       ++i) {
    SCOPED_TRACE(testing::Message() << "frame #" << i);

    // Move time forward to the next display vsync.
    AdvanceClockToNextVsync();
    const base::TimeTicks expected_reference_time =
        task_runner_->NowTicks() + kVsyncInterval;

    // Change the content of the frame sink and notify the capturer of the
    // damage.
    const SkColor color = SkColorSetARGB(255, static_cast<uint8_t>(i << 4),
                                         static_cast<uint8_t>((i << 4) + 0x10),
                                         static_cast<uint8_t>((i << 4) + 0x20));
    frame_sink_.SetCopyOutputColor(color);
    task_runner_->FastForwardBy(kVsyncInterval / 4);
    const base::TimeTicks expected_capture_begin_time =
        task_runner_->NowTicks();
    NotifyFrameDamaged(gfx::Rect(size_set().source_size));

    // The frame sink should have received a CopyOutputRequest. Simulate a short
    // pause before the result is sent back to the capturer, and the capturer
    // should then deliver the frame.
    ASSERT_EQ(i + 1, frame_sink_.num_copy_results());
    task_runner_->FastForwardBy(kVsyncInterval / 4);
    const base::TimeTicks expected_capture_end_time = task_runner_->NowTicks();
    frame_sink_.SendCopyOutputResult(i);
    ASSERT_EQ(i + 1, consumer.num_frames_received());

    // Verify the frame is the right size, has the right content, and has
    // required metadata set.
    const scoped_refptr<VideoFrame> frame = consumer.TakeFrame(i);
    EXPECT_THAT(frame, IsLetterboxedFrame(
                           color, size_set().ExpectedContentRect(pixel_format_),
                           pixel_format_));
    EXPECT_EQ(size_set().capture_size, frame->coded_size());
    EXPECT_EQ(gfx::Rect(size_set().capture_size), frame->visible_rect());
    EXPECT_LT(last_timestamp, frame->timestamp());
    last_timestamp = frame->timestamp();
    const VideoFrameMetadata& metadata = frame->metadata();
    EXPECT_EQ(expected_capture_begin_time, *metadata.capture_begin_time);
    EXPECT_EQ(expected_capture_end_time, *metadata.capture_end_time);
    EXPECT_EQ(GetColorSpaceForPixelFormat(pixel_format_), frame->ColorSpace());
    // frame_duration is an estimate computed by the VideoCaptureOracle, so it
    // its exact value is not being checked here.
    EXPECT_TRUE(metadata.frame_duration.has_value());
    EXPECT_NEAR(media::limits::kMaxFramesPerSecond, *metadata.frame_rate,
                0.001);
    EXPECT_EQ(expected_reference_time, *metadata.reference_time);

    // Notify the capturer that the consumer is done with the frame.
    consumer.SendDoneNotification(i);

    if (HasFailure()) {
      break;
    }
  }

  StopCapture();
}

// Tests that frame capturing halts when too many frames are allocated, whether
// that is because there are too many copy requests in-flight or because the
// consumer has not finished consuming frames fast enough.
TEST_P(FrameSinkVideoCapturerTest, HaltsWhenPoolIsFull) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kVideoCaptureTarget))
      .WillRepeatedly(Return(&frame_sink_));

  capturer_->ChangeTarget(kVideoCaptureTarget,
                          /*sub_capture_target_version=*/0);

  NiceMock<MockConsumer> consumer;
  StartCapture(&consumer);
  // With the start, an immediate refresh occurred.
  const int num_refresh_frames = 1;
  ASSERT_EQ(num_refresh_frames, frame_sink_.num_copy_results());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  // Saturate the pool with CopyOutputRequests that have not yet executed.
  int num_frames = FrameSinkVideoCapturerImpl::kFramePoolCapacity;
  for (int i = num_refresh_frames; i < num_frames; ++i) {
    AdvanceClockToNextVsync();
    NotifyFrameDamaged(gfx::Rect(size_set().source_size));
    // The oracle should not be rejecting captures caused by compositor updates.
    ASSERT_FALSE(IsRefreshRetryTimerRunning());
  }
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());

  // Notifying the capturer of new compositor updates should cause no new copy
  // requests to be issued at this point. However, the refresh timer should be
  // scheduled to account for the capture of changed content that could not take
  // place.
  AdvanceClockToNextVsync();
  NotifyFrameDamaged(gfx::Rect(size_set().source_size));
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());
  EXPECT_TRUE(IsRefreshRetryTimerRunning());

  // Complete the first copy request. When notifying the capturer of another
  // compositor update, no new copy requests should be issued because the first
  // frame is still in the middle of being delivered/consumed.
  frame_sink_.SendCopyOutputResult(0);
  ASSERT_EQ(1, consumer.num_frames_received());
  AdvanceClockToNextVsync();
  NotifyFrameDamaged(gfx::Rect(size_set().source_size));
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());
  EXPECT_TRUE(IsRefreshRetryTimerRunning());

  // Notify the capturer that the first frame has been consumed. This will not
  // cause the capturer to issue new copy requests, since the just-delivered
  // frame will now be marked - the capturer will not return it to the pool at
  // this time, so the pool is still at capacity. The refresh timer should still
  // be running.
  EXPECT_TRUE(consumer.TakeFrame(0));
  consumer.SendDoneNotification(0);
  AdvanceClockToNextVsync();
  NotifyFrameDamaged(gfx::Rect(size_set().source_size));
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());
  EXPECT_TRUE(IsRefreshRetryTimerRunning());

  // Complete the second copy request and notify the capturer that the second
  // frame has been consumed. Then, with another compositor update, the capturer
  // should issue a new copy request. The refresh timer should no longer be
  // running because the next capture will satisfy the need to send updated
  // content to the consumer. The frame produced by this CopyOutputRequest will
  // now be marked for resurrection.
  frame_sink_.SendCopyOutputResult(1);
  ASSERT_EQ(2, consumer.num_frames_received());
  EXPECT_TRUE(consumer.TakeFrame(1));
  consumer.SendDoneNotification(1);
  AdvanceClockToNextVsync();
  NotifyFrameDamaged(gfx::Rect(size_set().source_size));
  ++num_frames;
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  // With yet another compositor update, no new copy requests should be issued
  // because the pipeline became saturated again. Once again, the refresh timer
  // should be started to account for the need to capture at some future point.
  AdvanceClockToNextVsync();
  NotifyFrameDamaged(gfx::Rect(size_set().source_size));
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());
  EXPECT_TRUE(IsRefreshRetryTimerRunning());

  // Complete all pending copy requests. The frame for the most recently
  // delivered CopyOutputRequest becomes marked. This causes frame for COR[1] to
  // become unmarked, which drops the last reference to it, so the compositor
  // update will cause additional CopyOutputRequest to be issued. The refresh
  // timer will not be running.
  for (int i = 2; i < frame_sink_.num_copy_results(); ++i) {
    SCOPED_TRACE(testing::Message() << "frame #" << i);
    frame_sink_.SendCopyOutputResult(i);
  }
  ASSERT_EQ(frame_sink_.num_copy_results(), consumer.num_frames_received());
  AdvanceClockToNextVsync();
  NotifyFrameDamaged(gfx::Rect(size_set().source_size));
  ++num_frames;
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  // Complete the newly issued COR:
  frame_sink_.SendCopyOutputResult(frame_sink_.num_copy_results() - 1);

  // Notify the capturer that all frames have been consumed.
  for (int i = 2; i < consumer.num_frames_received(); ++i) {
    SCOPED_TRACE(testing::Message() << "frame #" << i);
    EXPECT_TRUE(consumer.TakeFrame(i));
    consumer.SendDoneNotification(i);
  }
  AdvanceClockToNextVsync();
  NotifyFrameDamaged(gfx::Rect(size_set().source_size));
  ++num_frames;
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  frame_sink_.SendCopyOutputResult(frame_sink_.num_copy_results() - 1);
  ASSERT_EQ(frame_sink_.num_copy_results(), consumer.num_frames_received());

  StopCapture();
}

// Tests that copy requests completed out-of-order are accounted for by the
// capturer, with results delivered to the consumer in-order.
TEST_P(FrameSinkVideoCapturerTest, DeliversFramesInOrder) {
  std::vector<SkColor> colors;
  colors.push_back(SkColorSetARGB(255, 0, 0, 0));
  frame_sink_.SetCopyOutputColor(colors.back());
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kVideoCaptureTarget))
      .WillRepeatedly(Return(&frame_sink_));

  capturer_->ChangeTarget(kVideoCaptureTarget,
                          /*sub_capture_target_version=*/0);

  NiceMock<MockConsumer> consumer;
  StartCapture(&consumer);

  // Simulate five compositor updates. Each composited frame has its content
  // region set to a different color to check that the video frames are being
  // delivered in-order.
  constexpr int num_refresh_frames = 1;
  constexpr int num_frames = 5;
  ASSERT_EQ(num_refresh_frames, frame_sink_.num_copy_results());
  for (int i = num_refresh_frames; i < num_frames; ++i) {
    colors.push_back(SkColorSetARGB(255, static_cast<uint8_t>(i << 4),
                                    static_cast<uint8_t>((i << 4) + 0x10),
                                    static_cast<uint8_t>((i << 4) + 0x20)));
    frame_sink_.SetCopyOutputColor(colors.back());
    AdvanceClockToNextVsync();
    NotifyFrameDamaged(gfx::Rect(size_set().source_size));
  }
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());

  // Complete the copy requests out-of-order. Check that frames are not
  // delivered until they can all be delivered in-order, and that the content of
  // each video frame is correct.
  const auto expected_content_rect =
      size_set().ExpectedContentRect(pixel_format_);
  frame_sink_.SendCopyOutputResult(0);
  ASSERT_EQ(1, consumer.num_frames_received());
  EXPECT_THAT(
      consumer.TakeFrame(0),
      IsLetterboxedFrame(colors[0], expected_content_rect, pixel_format_));
  frame_sink_.SendCopyOutputResult(2);
  ASSERT_EQ(1, consumer.num_frames_received());  // Waiting for frame 1.
  frame_sink_.SendCopyOutputResult(3);
  ASSERT_EQ(1, consumer.num_frames_received());  // Still waiting for frame 1.
  frame_sink_.SendCopyOutputResult(1);
  ASSERT_EQ(4, consumer.num_frames_received());  // Sent frames 1, 2, and 3.
  EXPECT_THAT(
      consumer.TakeFrame(1),
      IsLetterboxedFrame(colors[1], expected_content_rect, pixel_format_));
  EXPECT_THAT(
      consumer.TakeFrame(2),
      IsLetterboxedFrame(colors[2], expected_content_rect, pixel_format_));
  EXPECT_THAT(
      consumer.TakeFrame(3),
      IsLetterboxedFrame(colors[3], expected_content_rect, pixel_format_));
  frame_sink_.SendCopyOutputResult(4);
  ASSERT_EQ(5, consumer.num_frames_received());
  EXPECT_THAT(
      consumer.TakeFrame(4),
      IsLetterboxedFrame(colors[4], expected_content_rect, pixel_format_));

  StopCapture();
}

// Tests that in-flight copy requests are canceled when the capturer is
// stopped. When it is started again with a new consumer, only the results from
// newer copy requests should appear in video frames delivered to the consumer.
TEST_P(FrameSinkVideoCapturerTest, CancelsInFlightCapturesOnStop) {
  const SkColor color1 = SkColorSetARGB(255, 255, 95, 255);
  frame_sink_.SetCopyOutputColor(color1);
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kVideoCaptureTarget))
      .WillRepeatedly(Return(&frame_sink_));

  capturer_->ChangeTarget(kVideoCaptureTarget,
                          /*sub_capture_target_version=*/0);

  // Start capturing to the first consumer.
  MockConsumer consumer;
  EXPECT_CALL(consumer, OnFrameCapturedMock()).Times(2);
  EXPECT_CALL(consumer, OnStopped()).Times(1);
  StartCapture(&consumer);
  // With the start, an immediate refresh should have occurred.
  const int num_refresh_frames = 1;
  ASSERT_EQ(num_refresh_frames, frame_sink_.num_copy_results());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  // Simulate two compositor updates following the initial refresh.
  int num_copy_requests = 3;
  for (int i = num_refresh_frames; i < num_copy_requests; ++i) {
    SCOPED_TRACE(testing::Message() << "frame #" << i);
    AdvanceClockToNextVsync();
    NotifyFrameDamaged(gfx::Rect(size_set().source_size));
  }
  ASSERT_EQ(num_copy_requests, frame_sink_.num_copy_results());

  // Complete the first two copy requests.
  int num_completed_captures = 2;
  for (int i = 0; i < num_completed_captures; ++i) {
    SCOPED_TRACE(testing::Message() << "frame #" << i);
    frame_sink_.SendCopyOutputResult(i);
    ASSERT_EQ(i + 1, consumer.num_frames_received());
    EXPECT_THAT(consumer.TakeFrame(i),
                IsLetterboxedFrame(
                    color1, size_set().ExpectedContentRect(pixel_format_),
                    pixel_format_));
  }

  // Stopping capture should cancel the remaning copy requests.
  StopCapture();

  // Change the content color and start capturing to the second consumer.
  const SkColor color2 = SkColorSetARGB(255, 255, 91, 255);
  frame_sink_.SetCopyOutputColor(color2);
  MockConsumer consumer2;
  const int num_captures_for_second_consumer = 3;
  EXPECT_CALL(consumer2, OnFrameCapturedMock())
      .Times(num_captures_for_second_consumer);
  EXPECT_CALL(consumer2, OnStopped()).Times(1);
  StartCapture(&consumer2);
  // With the start, a refresh was attempted, but since the attempt occurred so
  // soon after the last frame capture, the oracle should have rejected it.
  // Thus, the refresh timer should be running.
  EXPECT_TRUE(IsRefreshRetryTimerRunning());

  // Complete the copy requests for the first consumer. Expect that they have no
  // effect on the second consumer.
  for (int i = num_completed_captures; i < num_copy_requests; ++i) {
    frame_sink_.SendCopyOutputResult(i);
    ASSERT_EQ(0, consumer2.num_frames_received());
  }

  // Reset the counter for |consumer2|.
  num_completed_captures = 0;

  // From here, any new copy requests should be executed with video frames
  // delivered to the consumer containing |color2|.
  for (int i = 0; i < num_captures_for_second_consumer; ++i) {
    AdvanceClockToNextVsync();
    if (i == 0) {
      // Expect that advancing the clock caused the refresh timer to fire.
    } else {
      NotifyFrameDamaged(gfx::Rect(size_set().source_size));
    }
    ++num_copy_requests;
    ASSERT_EQ(num_copy_requests, frame_sink_.num_copy_results());
    ASSERT_FALSE(IsRefreshRetryTimerRunning());
    frame_sink_.SendCopyOutputResult(frame_sink_.num_copy_results() - 1);
    ++num_completed_captures;
    ASSERT_EQ(num_completed_captures, consumer2.num_frames_received());
    EXPECT_THAT(consumer2.TakeFrame(consumer2.num_frames_received() - 1),
                IsLetterboxedFrame(
                    color2, size_set().ExpectedContentRect(pixel_format_),
                    pixel_format_));
  }

  StopCapture();
}

// Tests that refresh requests ultimately result in a frame being delivered to
// the consumer.
TEST_P(FrameSinkVideoCapturerTest, EventuallySendsARefreshFrame) {
  frame_sink_.SetCopyOutputColor(SkColorSetARGB(255, 128, 128, 128));
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kVideoCaptureTarget))
      .WillRepeatedly(Return(&frame_sink_));

  capturer_->ChangeTarget(kVideoCaptureTarget,
                          /*sub_capture_target_version=*/0);

  MockConsumer consumer;
  const int num_refresh_frames = 2;  // Initial, plus later refresh.
  const int num_update_frames = 3;
  EXPECT_CALL(consumer, OnFrameCapturedMock())
      .Times(num_refresh_frames + num_update_frames);
  EXPECT_CALL(consumer, OnStopped()).Times(1);
  StartCapture(&consumer);

  // With the start, an immediate refresh occurred. Simulate a copy result and
  // expect to see the refresh frame delivered to the consumer.
  ASSERT_EQ(1, frame_sink_.num_copy_results());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());
  frame_sink_.SendCopyOutputResult(0);
  ASSERT_EQ(1, consumer.num_frames_received());
  consumer.SendDoneNotification(0);

  // Drive the capturer pipeline for a series of frame composites.
  int num_frames = 1 + num_update_frames;
  for (int i = 1; i < num_frames; ++i) {
    AdvanceClockToNextVsync();
    NotifyFrameDamaged(gfx::Rect(size_set().source_size));
    ASSERT_EQ(i + 1, frame_sink_.num_copy_results());
    ASSERT_FALSE(IsRefreshRetryTimerRunning());
    frame_sink_.SendCopyOutputResult(i);
    ASSERT_EQ(i + 1, consumer.num_frames_received());
    consumer.SendDoneNotification(i);
  }

  // Request a refresh frame. Because the refresh request was made just after
  // the last frame capture, the refresh retry timer should be started.
  capturer_->RequestRefreshFrame();
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());
  EXPECT_TRUE(IsRefreshRetryTimerRunning());

  // Simulate the elapse of time and the firing of the refresh retry timer.
  // Since no compositor updates occurred in the meantime, this will execute a
  // passive refresh, which resurrects the last buffer instead of spawning an
  // additional copy request.
  AdvanceClockForRefreshTimer();
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());
  ASSERT_EQ(num_frames + 1, consumer.num_frames_received());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  StopCapture();
}

// Tests that refresh demands result in a frame being delivered to
// the consumer in a timely fashion.
TEST_P(FrameSinkVideoCapturerTest, RefreshDemandsAreProperlyHandled) {
  frame_sink_.SetCopyOutputColor(SkColorSetARGB(255, 128, 128, 128));
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kVideoCaptureTarget))
      .WillRepeatedly(Return(&frame_sink_));

  capturer_->ChangeTarget(kVideoCaptureTarget,
                          /*sub_capture_target_version=*/0);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnFrameCapturedMock()).Times(3);
  EXPECT_CALL(consumer, OnStopped()).Times(1);
  StartCapture(&consumer);

  // With the start, an immediate refresh occurred. Simulate a copy result and
  // expect to see the refresh frame delivered to the consumer.
  ASSERT_EQ(1, frame_sink_.num_copy_results());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());
  frame_sink_.SendCopyOutputResult(0);
  ASSERT_EQ(1, consumer.num_frames_received());
  consumer.SendDoneNotification(0);

  // Demand a refresh frame. We should be past the minimum time to add one, so
  // it should be done immediately.
  AdvanceClockToNextVsync();
  PropagateMojoTasks();
  capturer_->RefreshNow();
  PropagateMojoTasks();
  ASSERT_EQ(1, frame_sink_.num_copy_results());
  ASSERT_EQ(2, consumer.num_frames_received());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  // Demand again. Because we just got a frame, the refresh timer should be
  // started instead of capturing immediately.
  PropagateMojoTasks();
  capturer_->RefreshNow();
  PropagateMojoTasks();
  ASSERT_EQ(1, frame_sink_.num_copy_results());
  ASSERT_EQ(2, consumer.num_frames_received());
  EXPECT_TRUE(IsRefreshRetryTimerRunning());
  AdvanceClockForRefreshTimer();

  PropagateMojoTasks();
  ASSERT_EQ(1, frame_sink_.num_copy_results());
  ASSERT_EQ(3, consumer.num_frames_received());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  StopCapture();
}

// Tests that the capturer honors requested refresh frames (see
// crbug.com/1320798)
TEST_P(FrameSinkVideoCapturerTest, HonorsRequestRefreshFrame) {
  frame_sink_.SetCopyOutputColor(SkColorSetARGB(255, 128, 128, 128));
  ON_CALL(frame_sink_manager_, FindCapturableFrameSink(kVideoCaptureTarget))
      .WillByDefault(Return(&frame_sink_));

  capturer_->ChangeTarget(kVideoCaptureTarget,
                          /*sub_capture_target_version=*/0);

  // Start off and consume the immediate refresh and copy result.
  MockConsumer consumer;
  StartCapture(&consumer);
  frame_sink_.SendCopyOutputResult(0);
  ASSERT_EQ(1, consumer.num_frames_received());
  consumer.SendDoneNotification(0);

  // Advance time to avoid being frame rate limited by the oracle.
  // Demand a refresh frame. We should be past the minimum time to add one, so
  // it should be done immediately.
  AdvanceClockToNextVsync();
  capturer_->RefreshNow();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2, consumer.num_frames_received());

  // Advance time to avoid being frame rate limited by the oracle.
  // Request a refresh frame. The request should be serviced immediately.
  AdvanceClockToNextVsync();
  capturer_->RequestRefreshFrame();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3, consumer.num_frames_received());

  // Advance time to avoid being frame rate limited by the oracle.
  // Request again and expect service.
  AdvanceClockToNextVsync();
  capturer_->RequestRefreshFrame();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(4, consumer.num_frames_received());
}

// Tests that full capture happens on capture resolution change due to oracle,
// but only once and resurrected frames are used after that.
TEST_P(FrameSinkVideoCapturerTest,
       ResurrectsFramesForChangingCaptureResolution) {
  frame_sink_.SetCopyOutputColor(SkColorSetARGB(255, 128, 128, 128));
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kVideoCaptureTarget))
      .WillRepeatedly(Return(&frame_sink_));

  capturer_->ChangeTarget(kVideoCaptureTarget,
                          /*sub_capture_target_version=*/0);

  MockConsumer consumer;
  constexpr int num_refresh_frames = 3;  // Initial, plus two refreshes after
                                         // downscale and upscale.
  constexpr int num_update_frames = 3;

  int expected_frames_count = 0;
  int expected_copy_results = 0;

  EXPECT_CALL(consumer, OnFrameCapturedMock())
      .Times(num_refresh_frames + num_update_frames);
  EXPECT_CALL(consumer, OnStopped()).Times(1);
  StartCapture(&consumer);

  // 1. The first frame captured automatically once the capture stats.
  // It will be marked as the latest content in the buffer.
  EXPECT_FALSE(IsRefreshRetryTimerRunning());
  ++expected_copy_results;
  ++expected_frames_count;
  frame_sink_.SendCopyOutputResult(expected_copy_results - 1);
  ASSERT_EQ(expected_copy_results, frame_sink_.num_copy_results());
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());
  consumer.SendDoneNotification(expected_copy_results - 1);

  // 2. Another frame is captured, but there was no updates since the previous
  // frame, therefore the marked frame should be resurrected, without making an
  // actual request.
  capturer_->RequestRefreshFrame();
  AdvanceClockForRefreshTimer();
  ++expected_frames_count;
  EXPECT_EQ(expected_copy_results, frame_sink_.num_copy_results());
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());
  // If we do not advance the clock, the oracle will reject capture due to
  // frame rate limits.
  AdvanceClockToNextVsync();

  // 3. Simulate a change in the oracle imposed capture size (e.g. due to
  // overuse). This frame is of a different size than the cached frame and will
  // be captured with a CopyOutputRequest.
  ForceOracleSize(kSizeSets[3]);
  capturer_->RequestRefreshFrame();
  AdvanceClockForRefreshTimer();
  ++expected_copy_results;
  ++expected_frames_count;
  frame_sink_.SendCopyOutputResult(expected_copy_results - 1);
  ASSERT_EQ(expected_copy_results, frame_sink_.num_copy_results());
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());
  consumer.SendDoneNotification(expected_copy_results - 1);

  // 4. Another frame is captured, but there was no updates since the previous
  // frame, therefore the marked frame should be resurrected, without making an
  // actual request.
  capturer_->RequestRefreshFrame();
  AdvanceClockForRefreshTimer();
  ++expected_frames_count;
  EXPECT_EQ(expected_copy_results, frame_sink_.num_copy_results());
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());
  // If we do not advance the clock, the oracle will reject capture due to
  // frame rate limits.
  AdvanceClockToNextVsync();

  // 5. Simulate a change in the oracle imposed capture size (e.g. due to
  // overuse). This frame is of a different size than the cached frame and will
  // be captured with a CopyOutputRequest.
  ForceOracleSize(kSizeSets[0]);
  capturer_->RequestRefreshFrame();
  AdvanceClockForRefreshTimer();
  ++expected_copy_results;
  ++expected_frames_count;
  frame_sink_.SendCopyOutputResult(expected_copy_results - 1);
  ASSERT_EQ(expected_copy_results, frame_sink_.num_copy_results());
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());
  consumer.SendDoneNotification(expected_copy_results - 1);

  // 6. Another frame is captured, but there was no updates since the previous
  // frame, therefore the marked frame should be resurrected, without making an
  // actual request.
  capturer_->RequestRefreshFrame();
  AdvanceClockForRefreshTimer();
  ++expected_frames_count;
  EXPECT_EQ(expected_copy_results, frame_sink_.num_copy_results());
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  StopCapture();
}

// Tests that CompositorFrameMetadata variables (|device_scale_factor|,
// |page_scale_factor| and |root_scroll_offset|) are sent along with each frame,
// and refreshes cause variables of the cached CompositorFrameMetadata
// (|last_frame_metadata|) to be used.
TEST_P(FrameSinkVideoCapturerTest, CompositorFrameMetadataReachesConsumer) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kVideoCaptureTarget))
      .WillRepeatedly(Return(&frame_sink_));
  capturer_->ChangeTarget(kVideoCaptureTarget,
                          /*sub_capture_target_version=*/0);

  MockConsumer consumer;
  // Initial refresh frame for starting capture, plus later refresh.
  const int num_refresh_frames = 2;
  const int num_update_frames = 1;
  EXPECT_CALL(consumer, OnFrameCapturedMock())
      .Times(num_refresh_frames + num_update_frames);
  EXPECT_CALL(consumer, OnStopped()).Times(1);
  StartCapture(&consumer);

  // With the start, an immediate refresh occurred. Simulate a copy result.
  // Expect to see the refresh frame delivered to the consumer, along with
  // default metadata values.
  int cur_frame_index = 0, expected_frames_count = 1;
  frame_sink_.SendCopyOutputResult(cur_frame_index);
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());
  EXPECT_TRUE(CompareVarsInCompositorFrameMetadata(
      *(consumer.TakeFrame(cur_frame_index)), kDefaultDeviceScaleFactor,
      kDefaultPageScaleFactor, kDefaultRootScrollOffset));
  consumer.SendDoneNotification(cur_frame_index);

  // The metadata used to signal a frame damage and verify that it reaches the
  // consumer.
  const float kNewDeviceScaleFactor = 3.5;
  const float kNewPageScaleFactor = 1.5;
  const gfx::PointF kNewRootScrollOffset = gfx::PointF(100, 200);

  // Notify frame damage with new metadata, and expect that the refresh frame
  // is delivered to the consumer with this new metadata.
  AdvanceClockToNextVsync();
  NotifyFrameDamaged(gfx::Rect(size_set().source_size), kNewDeviceScaleFactor,
                     kNewPageScaleFactor, kNewRootScrollOffset);
  frame_sink_.SendCopyOutputResult(++cur_frame_index);
  ++expected_frames_count;
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());
  EXPECT_TRUE(CompareVarsInCompositorFrameMetadata(
      *(consumer.TakeFrame(cur_frame_index)), kNewDeviceScaleFactor,
      kNewPageScaleFactor, kNewRootScrollOffset));
  consumer.SendDoneNotification(cur_frame_index);

  // Request a refresh frame. Because the refresh request was made just after
  // the last frame capture, the refresh retry timer should be started.
  // Expect that the refresh frame is delivered to the consumer with the same
  // metadata from the previous frame.
  capturer_->RequestRefreshFrame();
  AdvanceClockForRefreshTimer();
  ++expected_frames_count;
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());
  EXPECT_TRUE(CompareVarsInCompositorFrameMetadata(
      *(consumer.TakeFrame(++cur_frame_index)), kNewDeviceScaleFactor,
      kNewPageScaleFactor, kNewRootScrollOffset));
  StopCapture();
}

// Tests that frame metadata CAPTURE_COUNTER and CAPTURE_UPDATE_RECT are sent to
// the consumer as part of each frame delivery.
TEST_P(FrameSinkVideoCapturerTest, DeliversUpdateRectAndCaptureCounter) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kVideoCaptureTarget))
      .WillRepeatedly(Return(&frame_sink_));
  capturer_->ChangeTarget(kVideoCaptureTarget,
                          /*sub_capture_target_version=*/0);

  MockConsumer consumer;
  StartCapture(&consumer);

  // With the start, an immediate refresh occurred. Simulate a copy result.
  // Expect to see the refresh frame delivered to the consumer, along with
  // default metadata values.
  int cur_capture_index = 0, cur_frame_index = 0, expected_frames_count = 1,
      previous_capture_counter_received = 0;
  frame_sink_.SendCopyOutputResult(cur_capture_index);
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());
  {
    auto received_frame = consumer.TakeFrame(cur_frame_index);
    EXPECT_EQ(gfx::Rect(size_set().capture_size),
              received_frame->metadata().capture_update_rect);
    previous_capture_counter_received =
        *received_frame->metadata().capture_counter;
  }
  consumer.SendDoneNotification(cur_frame_index);

  const gfx::Rect kSourceDamageRect = gfx::Rect(3, 7, 60, 45);
  gfx::Rect expected_frame_update_rect = copy_output::ComputeResultRect(
      kSourceDamageRect,
      gfx::Vector2d(size_set().source_size.width(),
                    size_set().source_size.height()),
      gfx::Vector2d(size_set().ExpectedContentRect(pixel_format_).width(),
                    size_set().ExpectedContentRect(pixel_format_).height()));
  expected_frame_update_rect.Offset(
      size_set().ExpectedContentRect(pixel_format_).OffsetFromOrigin());
  // Do not align when we are testing RGBA
  if (pixel_format_ != media::PIXEL_FORMAT_ARGB) {
    EXPECT_FALSE(
        AlignsWithI420SubsamplingBoundaries(expected_frame_update_rect));
    expected_frame_update_rect =
        ExpandRectToI420SubsampleBoundaries(expected_frame_update_rect);
    EXPECT_TRUE(
        AlignsWithI420SubsamplingBoundaries(expected_frame_update_rect));
  }

  // Notify frame damage with custom damage rect, and expect that the refresh
  // frame is delivered to the consumer with a corresponding |update_rect|.
  AdvanceClockToNextVsync();
  NotifyFrameDamaged(kSourceDamageRect);
  frame_sink_.SendCopyOutputResult(++cur_capture_index);
  ++expected_frames_count;
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());
  {
    auto received_frame = consumer.TakeFrame(++cur_frame_index);
    int received_capture_counter = *received_frame->metadata().capture_counter;
    EXPECT_EQ(expected_frame_update_rect,
              *received_frame->metadata().capture_update_rect);
    EXPECT_EQ(previous_capture_counter_received + 1, received_capture_counter);
    previous_capture_counter_received = received_capture_counter;
  }
  consumer.SendDoneNotification(cur_frame_index);

  // Request a refresh frame. Because the refresh request was made just after
  // the last frame capture, the refresh retry timer should be started.
  // Since there was no damage to the source, expect that the |update_region|
  // delivered to the consumer is empty.
  capturer_->RequestRefreshFrame();
  AdvanceClockForRefreshTimer();
  ++expected_frames_count;
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());
  {
    auto received_frame = consumer.TakeFrame(++cur_frame_index);
    int received_capture_counter = *received_frame->metadata().capture_counter;
    EXPECT_TRUE(received_frame->metadata().capture_update_rect->IsEmpty());
    EXPECT_EQ(previous_capture_counter_received + 1, received_capture_counter);
    previous_capture_counter_received = received_capture_counter;
  }
  consumer.SendDoneNotification(cur_frame_index);

  // If we do not advance the clock, the oracle will reject capture due to
  // frame rate limits.
  AdvanceClockToNextVsync();
  // Simulate a change in the source size.
  // This is expected to trigger a refresh capture.
  SwitchToSizeSet(kSizeSets[1]);
  frame_sink_.SendCopyOutputResult(++cur_capture_index);
  ++expected_frames_count;
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());
  {
    auto received_frame = consumer.TakeFrame(++cur_frame_index);
    int received_capture_counter = *received_frame->metadata().capture_counter;
    EXPECT_EQ(gfx::Rect(size_set().capture_size),
              *received_frame->metadata().capture_update_rect);
    EXPECT_EQ(previous_capture_counter_received + 1, received_capture_counter);
    previous_capture_counter_received = received_capture_counter;
  }
  consumer.SendDoneNotification(cur_frame_index);

  // If we do not advance the clock, the oracle will reject capture due to
  // frame rate.
  AdvanceClockToNextVsync();
  // Simulate a change in the capture size.
  // This is expected to trigger a refresh capture.
  SwitchToSizeSet(kSizeSets[2]);
  frame_sink_.SendCopyOutputResult(++cur_capture_index);
  ++expected_frames_count;
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());
  {
    auto received_frame = consumer.TakeFrame(++cur_frame_index);
    int received_capture_counter = *received_frame->metadata().capture_counter;
    EXPECT_EQ(gfx::Rect(size_set().capture_size),
              *received_frame->metadata().capture_update_rect);
    EXPECT_EQ(previous_capture_counter_received + 1, received_capture_counter);
    previous_capture_counter_received = received_capture_counter;
  }

  StopCapture();
}

// Tests that when captured frames being dropped before delivery, the
// CAPTURE_COUNTER metadata value sent to the consumer reflects the frame drops
// indicating that CAPTURE_UPDATE_RECT must be ignored.
TEST_P(FrameSinkVideoCapturerTest, CaptureCounterSkipsWhenFramesAreDropped) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kVideoCaptureTarget))
      .WillRepeatedly(Return(&frame_sink_));
  capturer_->ChangeTarget(kVideoCaptureTarget,
                          /*sub_capture_target_version=*/0);

  MockConsumer consumer;
  StartCapture(&consumer);

  // With the start, an immediate refresh occurred. Simulate a copy result.
  // Expect to see the refresh frame delivered to the consumer, along with
  // default metadata values.
  int cur_capture_frame_index = 0, cur_receive_frame_index = 0,
      expected_frames_count = 1, previous_capture_counter_received = 0;
  frame_sink_.SendCopyOutputResult(cur_capture_frame_index);
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());
  {
    auto received_frame = consumer.TakeFrame(cur_receive_frame_index);
    EXPECT_EQ(gfx::Rect(size_set().capture_size),
              *received_frame->metadata().capture_update_rect);
    previous_capture_counter_received =
        *received_frame->metadata().capture_counter;
  }
  consumer.SendDoneNotification(cur_receive_frame_index);

  const gfx::Rect kArbitraryDamageRect1 = gfx::Rect(1, 2, 6, 6);
  const gfx::Rect kArbitraryDamageRect2 = gfx::Rect(3, 7, 5, 5);

  // Notify frame damage with custom damage rect, but have oracle reject frame
  // delivery. Expect that no frame is sent to the consumer.
  oracle_->set_return_false_on_complete_capture(true);
  AdvanceClockToNextVsync();
  NotifyFrameDamaged(kArbitraryDamageRect1);
  frame_sink_.SendCopyOutputResult(++cur_capture_frame_index);
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());

  // Another frame damage with a different rect is reported. This time the
  // oracle accepts frame delivery.
  oracle_->set_return_false_on_complete_capture(false);
  AdvanceClockToNextVsync();
  NotifyFrameDamaged(kArbitraryDamageRect2);
  frame_sink_.SendCopyOutputResult(++cur_capture_frame_index);
  ++expected_frames_count;
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());
  {
    auto received_frame = consumer.TakeFrame(++cur_receive_frame_index);
    EXPECT_NE(previous_capture_counter_received + 1,
              *received_frame->metadata().capture_counter);
  }
  StopCapture();
}

TEST_P(FrameSinkVideoCapturerTest, ClientCaptureStartsAndStops) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kVideoCaptureTarget))
      .WillRepeatedly(Return(&frame_sink_));

  capturer_->ChangeTarget(kVideoCaptureTarget,
                          /*sub_capture_target_version=*/0);
  EXPECT_EQ(frame_sink_.number_clients_capturing(), 0);

  // Start capturing. frame_sink_ should now have one client capturing.
  NiceMock<MockConsumer> consumer;
  StartCapture(&consumer);
  EXPECT_EQ(frame_sink_.number_clients_capturing(), 1);

  // Stop capturing. frame_sink_ should now have no client capturing.
  StopCapture();
  EXPECT_EQ(frame_sink_.number_clients_capturing(), 0);
}

TEST_P(FrameSinkVideoCapturerTest, RegionCaptureCropId) {
  SwitchToSizeSet(kSizeSets[4]);
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kVideoCaptureTarget))
      .WillRepeatedly(Return(&frame_sink_));
  capturer_->ChangeTarget(kVideoCaptureTarget,
                          /*sub_capture_target_version=*/0);
  EXPECT_EQ(frame_sink_.number_clients_capturing(), 0);

  const auto kCropId = RegionCaptureCropId::CreateRandom();
  constexpr gfx::Rect kCropBounds{1, 2, 640, 478};

  VideoCaptureTarget target(kVideoCaptureTarget.frame_sink_id, kCropId);
  frame_sink_.set_crop_bounds(kCropBounds);
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(target))
      .WillRepeatedly(Return(&frame_sink_));
  capturer_->ChangeTarget(std::move(target), /*sub_capture_target_version=*/1);

  // Start capturing. frame_sink_ should now have one client capturing.
  NiceMock<MockConsumer> consumer;
  StartCapture(&consumer);
  EXPECT_EQ(frame_sink_.number_clients_capturing(), 1);

  // The frame sink's current crop ID should have been set as a side-effect.
  EXPECT_EQ(kCropId, frame_sink_.current_crop_id());
}

TEST_P(FrameSinkVideoCapturerTest,
       RegionCaptureTargetIsSetLaterWhenNotInitiallyAvailable) {
  SwitchToSizeSet(kSizeSets[4]);

  const auto kCropId = RegionCaptureCropId::CreateRandom();
  constexpr gfx::Rect kCropBounds{1, 2, 640, 478};

  // The region capture crop identifier is not associated with any frame
  // sinks yet.
  VideoCaptureTarget target(kVideoCaptureTarget.frame_sink_id, kCropId);
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(target))
      .WillRepeatedly(Return(nullptr));
  capturer_->ChangeTarget(target, /*sub_capture_target_version=*/0);

  // Start capture, although we don't have a frame sink yet.
  NiceMock<MockConsumer> consumer;
  StartCapture(&consumer);
  EXPECT_EQ(frame_sink_.number_clients_capturing(), 0);

  // A frame sink that has |kCropId| associated with it should now be
  // available, and the capturer should automatically attach to it.
  frame_sink_.set_crop_bounds(kCropBounds);
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(target))
      .WillRepeatedly(Return(&frame_sink_));
  EXPECT_TRUE(IsRefreshRetryTimerRunning());
  AdvanceClockForRefreshTimer();

  EXPECT_EQ(frame_sink_.number_clients_capturing(), 1);
  EXPECT_EQ(kCropId, frame_sink_.current_crop_id());
}

// Tests that frames can be successfully delivered after one is dropped due to
// having a zero-sized capture region.
TEST_P(FrameSinkVideoCapturerTest, HandlesFrameWithEmptyRegion) {
  const auto kCropId = RegionCaptureCropId::CreateRandom();
  constexpr gfx::Rect kValidCropBounds{10, 2, 630, 476};

  SwitchToSizeSet(kSizeSets[4]);
  VideoCaptureTarget target(kVideoCaptureTarget.frame_sink_id, kCropId);
  frame_sink_.set_crop_bounds(gfx::Rect{});
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(target))
      .WillRepeatedly(Return(&frame_sink_));

  NiceMock<MockConsumer> consumer;
  EXPECT_CALL(consumer, OnFrameCapturedMock()).Times(0);
  EXPECT_CALL(consumer, OnStopped()).Times(1);

  // Start capturing. frame_sink_ should now have one client capturing.
  StartCapture(&consumer);
  capturer_->ChangeTarget(target, /*sub_capture_target_version=*/0);
  EXPECT_EQ(frame_sink_.number_clients_capturing(), 1);

  // No copy requests should have been issued/executed.
  EXPECT_EQ(0, frame_sink_.num_copy_results());

  // The refresh timer should be running--our initial attempt to get a frame
  // failed due to being cropped to zero.
  EXPECT_TRUE(IsRefreshRetryTimerRunning());

  // Simulate several refresh timer intervals elapsing and the timer firing.
  // Nothing should happen because the frames should be cropped to zero.
  for (int i = 0; i < 5; ++i) {
    AdvanceClockForRefreshTimer();
    ASSERT_EQ(0, frame_sink_.num_copy_results());
    ASSERT_TRUE(IsRefreshRetryTimerRunning());
  }
  // We should only get one notification--the first empty frame.
  EXPECT_EQ(1, consumer.num_frames_with_empty_region());

  // Now, set the crop bounds to be valid--meaning completely contained inside
  // of the source rect. As it resolves, the next refresh capture should trigger
  // a copy request.
  frame_sink_.set_crop_bounds(kValidCropBounds);
  AdvanceClockForRefreshTimer();
  EXPECT_EQ(1, frame_sink_.num_copy_results());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());
  EXPECT_EQ(1, consumer.num_frames_with_empty_region());

  // The empty frame notification for the consumer is reset when the frame
  // is delivered to the consumer, so we need to have a result before it gets
  // reset.
  EXPECT_CALL(consumer, OnFrameCapturedMock()).Times(1);
  frame_sink_.SendCopyOutputResult(0);

  // Now, set back to an entirely empty crop bounds--we should get a
  // notification that we have an empty region.
  frame_sink_.set_crop_bounds(gfx::Rect{});
  capturer_->RequestRefreshFrame();
  AdvanceClockForRefreshTimer();
  EXPECT_EQ(1, frame_sink_.num_copy_results());
  EXPECT_TRUE(IsRefreshRetryTimerRunning());
  EXPECT_EQ(2, consumer.num_frames_with_empty_region());

  StopCapture();
  EXPECT_FALSE(IsRefreshRetryTimerRunning());
}

// Tests that frames can be successfully delivered after one is dropped due to
// having a capture region that does not intersect with the compositor frame. In
// the past, it was possible for a dropped frame to cause the delivery queue to
// no longer be emptied. See https://crbug.com/1300742.
TEST_P(FrameSinkVideoCapturerTest, HandlesFrameWithRegionCroppedToZero) {
  const auto kCropId = RegionCaptureCropId::CreateRandom();
  constexpr gfx::Rect kInvalidCropBounds{800, 600, 100, 100};
  constexpr gfx::Rect kValidCropBounds{1, 2, 638, 476};

  SwitchToSizeSet(kSizeSets[4]);
  VideoCaptureTarget target(kVideoCaptureTarget.frame_sink_id, kCropId);
  frame_sink_.set_crop_bounds(kInvalidCropBounds);
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(target))
      .WillRepeatedly(Return(&frame_sink_));

  NiceMock<MockConsumer> consumer;
  EXPECT_CALL(consumer, OnFrameCapturedMock()).Times(0);
  EXPECT_CALL(consumer, OnStopped()).Times(1);

  // Start capturing. frame_sink_ should now have one client capturing.
  StartCapture(&consumer);
  capturer_->ChangeTarget(target, /*sub_capture_target_version=*/0);
  EXPECT_EQ(frame_sink_.number_clients_capturing(), 1);

  // No copy requests should have been issued/executed.
  EXPECT_EQ(0, frame_sink_.num_copy_results());

  // The refresh timer should be running--our initial attempt to get a frame
  // failed due to being cropped to zero.
  EXPECT_TRUE(IsRefreshRetryTimerRunning());

  // Simulate several refresh timer intervals elapsing and the timer firing.
  // Nothing should happen because the frames should be cropped to zero.
  for (int i = 0; i < 5; ++i) {
    AdvanceClockForRefreshTimer();
    ASSERT_EQ(0, frame_sink_.num_copy_results());
    ASSERT_TRUE(IsRefreshRetryTimerRunning());
  }

  // Now, set the crop bounds to be valid--meaning completely contained inside
  // of the source rect. As it resolves, the next refresh capture should trigger
  // a copy request.
  frame_sink_.set_crop_bounds(kValidCropBounds);
  AdvanceClockForRefreshTimer();
  EXPECT_EQ(1, frame_sink_.num_copy_results());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  StopCapture();
  EXPECT_FALSE(IsRefreshRetryTimerRunning());
}

TEST_P(FrameSinkVideoCapturerTest, ProperlyHandlesCaptureSizeForOverlay) {
  // Skip this test for GMB, as it is not rendered by us.
  if (IsUsingGpuMemoryBuffer()) {
    return;
  }

  SwitchToSizeSet(kSizeSets[4]);
  constexpr gfx::Rect kValidCropBounds{1, 2, 638, 476};
  const auto kCropId = RegionCaptureCropId::CreateRandom();

  // First, create the overlay.
  mojo::Remote<mojom::FrameSinkVideoCaptureOverlay> overlay_remote;
  std::optional<VideoCaptureOverlay::CapturedFrameProperties> frame_properties;
  auto test_overlay = std::make_unique<TestVideoCaptureOverlay>(
      *capturer_, overlay_remote.BindNewPipeAndPassReceiver(),
      base::BindLambdaForTesting(
          [&](const VideoCaptureOverlay::CapturedFrameProperties& properties) {
            frame_properties = properties;
          }));
  InsertOverlay(std::move(test_overlay));

  // Change to the appropriate target.
  VideoCaptureTarget target(kVideoCaptureTarget.frame_sink_id, kCropId);
  frame_sink_.set_crop_bounds(kValidCropBounds);
  frame_sink_.SetCopyOutputColor(SkColorSetARGB(255, 128, 128, 128));
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(target))
      .WillRepeatedly(Return(&frame_sink_));
  capturer_->ChangeTarget(std::move(target), /*sub_capture_target_version=*/0);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnFrameCapturedMock());
  StartCapture(&consumer);

  // With the start, an immediate refresh occurred. Simulate a copy result and
  // expect to see the refresh frame delivered to the consumer.
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  ASSERT_EQ(1, frame_sink_.num_copy_results());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());
  frame_sink_.SendCopyOutputResult(0);
  ASSERT_EQ(1, consumer.num_frames_received());
  consumer.SendDoneNotification(0);

  // The overlay should have been rendered with the compositor region using
  // the entire frame, which is larger than the sub region.
  EXPECT_TRUE(frame_properties) << "didn't produce an overlay.";
  EXPECT_EQ(kSizeSets[4].source_size,
            frame_properties->region_properties.root_render_pass_size);
  EXPECT_EQ(kValidCropBounds,
            frame_properties->region_properties.render_pass_subrect);
  EXPECT_EQ((gfx::Rect{0, 2, 16, 12}), frame_properties->content_rect);
}

TEST_P(FrameSinkVideoCapturerTest, HandlesSubtreeCaptureId) {
  SwitchToSizeSet(kSizeSets[4]);
  constexpr gfx::Rect kCaptureBounds{1, 2, 1024, 768};
  constexpr SubtreeCaptureId kCaptureId(base::Token(0u, 1234567u));
  VideoCaptureTarget target(kVideoCaptureTarget.frame_sink_id, kCaptureId);
  frame_sink_.set_capture_bounds(kCaptureBounds);
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(target))
      .WillRepeatedly(Return(&frame_sink_));
  capturer_->ChangeTarget(std::move(target), /*sub_capture_target_version=*/0);

  // Start capturing. frame_sink_ should now have one client capturing.
  NiceMock<MockConsumer> consumer;
  StartCapture(&consumer);
  EXPECT_EQ(frame_sink_.number_clients_capturing(), 1);
  EXPECT_EQ((RegionCaptureCropId()), frame_sink_.current_crop_id());

  // The frame sink's capture ID should have been set as a side-effect.
  EXPECT_EQ(kCaptureId, frame_sink_.current_capture_id());
}

TEST_P(FrameSinkVideoCapturerTest, ProperlyHandlesSubtreeSizeForOverlay) {
  // Skip this test for GMB, as it is not rendered by us.
  if (IsUsingGpuMemoryBuffer()) {
    return;
  }

  SwitchToSizeSet(kSizeSets[4]);
  constexpr gfx::Rect kCaptureBounds{0, 0, 640, 478};
  constexpr SubtreeCaptureId kCaptureId(base::Token(0u, 1234567u));

  // First, create the overlay.
  mojo::Remote<mojom::FrameSinkVideoCaptureOverlay> overlay_remote;
  std::optional<VideoCaptureOverlay::CapturedFrameProperties> frame_properties;
  auto test_overlay = std::make_unique<TestVideoCaptureOverlay>(
      *capturer_, overlay_remote.BindNewPipeAndPassReceiver(),
      base::BindLambdaForTesting(
          [&](const VideoCaptureOverlay::CapturedFrameProperties& properties) {
            frame_properties = properties;
          }));
  InsertOverlay(std::move(test_overlay));

  // Change to the appropriate target.
  VideoCaptureTarget target(kVideoCaptureTarget.frame_sink_id, kCaptureId);
  frame_sink_.set_capture_bounds(kCaptureBounds);
  frame_sink_.SetCopyOutputColor(SkColorSetARGB(255, 128, 128, 128));
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(target))
      .WillRepeatedly(Return(&frame_sink_));
  capturer_->ChangeTarget(std::move(target), /*sub_capture_target_version=*/0);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnFrameCapturedMock());
  StartCapture(&consumer);

  // With the start, an immediate refresh occurred. Simulate a copy result and
  // expect to see the refresh frame delivered to the consumer.
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  ASSERT_EQ(1, frame_sink_.num_copy_results());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());
  frame_sink_.SendCopyOutputResult(0);
  ASSERT_EQ(1, consumer.num_frames_received());
  consumer.SendDoneNotification(0);

  // The overlay should have been rendered with the content and compositor
  // regions set to the same value.
  EXPECT_TRUE(frame_properties) << "didn't produce an overlay.";
  EXPECT_EQ(kCaptureBounds.size(),
            frame_properties->region_properties.root_render_pass_size);
  EXPECT_EQ(kCaptureBounds,
            frame_properties->region_properties.render_pass_subrect);
  EXPECT_EQ((gfx::Rect{0, 2, 16, 12}), frame_properties->content_rect);
}

TEST_P(FrameSinkVideoCapturerTest, HandlesNullSubTargetPtrCorrectly) {
  SwitchToSizeSet(kSizeSets[4]);
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kVideoCaptureTarget))
      .WillRepeatedly(Return(&frame_sink_));

  // The default cause is a target with no sub target, passed as nullptr. Since
  // the SubTarget is a mojom variant, the default SubTarget::New() is actually
  // a zero value subtree capture identifier.
  capturer_->ChangeTarget(VideoCaptureTarget(kVideoCaptureTarget),
                          /*sub_capture_target_version=*/0);
  EXPECT_EQ(frame_sink_.number_clients_capturing(), 0);

  // Start capturing. frame_sink_ should now have one client capturing.
  NiceMock<MockConsumer> consumer;
  StartCapture(&consumer);
  EXPECT_EQ(frame_sink_.number_clients_capturing(), 1);
  EXPECT_EQ(SubtreeCaptureId(), frame_sink_.current_capture_id());
  EXPECT_EQ(RegionCaptureCropId(), frame_sink_.current_crop_id());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FrameSinkVideoCapturerTest,
    testing::Values(
        std::tuple(mojom::BufferFormatPreference::kDefault,
                   media::PIXEL_FORMAT_I420),
        std::tuple(mojom::BufferFormatPreference::kDefault,
                   media::PIXEL_FORMAT_ARGB),
        std::tuple(mojom::BufferFormatPreference::kPreferGpuMemoryBuffer,
                   media::PIXEL_FORMAT_NV12),
        std::tuple(mojom::BufferFormatPreference::kPreferGpuMemoryBuffer,
                   media::PIXEL_FORMAT_ARGB)));

}  // namespace viz
