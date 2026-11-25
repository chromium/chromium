// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/services/recording/public/mojom/recording_service.mojom.h"
#include "chromeos/ash/services/recording/recording_service_test_api.h"
#include "chromeos/ash/services/recording/rgb_video_frame.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/video_capture_target.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace recording {

namespace {

constexpr gfx::Size kFrameSinkSize{160, 120};

class FakeVideoCapturer : public viz::mojom::FrameSinkVideoCapturer {
 public:
  FakeVideoCapturer() = default;
  FakeVideoCapturer(const FakeVideoCapturer&) = delete;
  FakeVideoCapturer& operator=(const FakeVideoCapturer&) = delete;
  ~FakeVideoCapturer() override = default;

  media::VideoPixelFormat requested_format() const { return requested_format_; }

  mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void set_on_started_closure(base::OnceClosure closure) {
    on_started_closure_ = std::move(closure);
  }

  void SendFrame(media::VideoPixelFormat pixel_format,
                 const gfx::Size& coded_size) {
    CHECK(consumer_);
    base::MappedReadOnlyRegion shmem = base::ReadOnlySharedMemoryRegion::Create(
        media::VideoFrame::AllocationSize(pixel_format, coded_size));
    CHECK(shmem.IsValid());
    std::ranges::fill(shmem.mapping.GetMemoryAsSpan<uint8_t>(), 0);

    auto info = media::mojom::VideoFrameInfo::New();
    info->timestamp = next_timestamp_;
    next_timestamp_ += base::Hertz(30);
    info->metadata.reference_time = base::TimeTicks::Now();
    info->pixel_format = pixel_format;
    info->coded_size = coded_size;
    info->visible_rect = gfx::Rect(coded_size);
    info->natural_size = coded_size;
    info->color_space = pixel_format == media::PIXEL_FORMAT_ARGB
                            ? gfx::ColorSpace::CreateSRGB()
                            : gfx::ColorSpace::CreateREC709();

    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks;
    std::ignore = callbacks.InitWithNewPipeAndPassReceiver();
    consumer_->OnFrameCaptured(
        media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
            std::move(shmem.region)),
        std::move(info), gfx::Rect(coded_size), std::move(callbacks));
    consumer_.FlushForTesting();
  }

  // viz::mojom::FrameSinkVideoCapturer:
  void SetFormat(media::VideoPixelFormat format) override {
    requested_format_ = format;
  }
  void SetMinCapturePeriod(base::TimeDelta min_period) override {}
  void SetMinSizeChangePeriod(base::TimeDelta min_period) override {}
  void SetResolutionConstraints(const gfx::Size& min_size,
                                const gfx::Size& max_size,
                                bool use_fixed_aspect_ratio) override {}
  void SetAutoThrottlingEnabled(bool enabled) override {}
  void SetAnimationFpsLockIn(bool enabled,
                             float majority_damaged_pixel_min_ratio) override {}
  void SetIsSecure(bool secure) override {}
  void ChangeTarget(const std::optional<viz::VideoCaptureTarget>& target,
                    uint32_t sub_capture_version) override {}
  void Start(
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumer> consumer,
      viz::mojom::BufferFormatPreference buffer_format_preference) override {
    consumer_.Bind(std::move(consumer));
    if (on_started_closure_) {
      std::move(on_started_closure_).Run();
    }
  }
  void Stop() override {
    if (consumer_) {
      consumer_->OnStopped();
    }
  }
  void RequestRefreshFrame() override {}
  void InvalidateBuffers() override {}
  void CreateOverlay(
      int32_t stacking_index,
      mojo::PendingReceiver<viz::mojom::FrameSinkVideoCaptureOverlay> receiver)
      override {}

 private:
  mojo::Receiver<viz::mojom::FrameSinkVideoCapturer> receiver_{this};
  mojo::Remote<viz::mojom::FrameSinkVideoConsumer> consumer_;
  media::VideoPixelFormat requested_format_ = media::PIXEL_FORMAT_UNKNOWN;
  base::OnceClosure on_started_closure_;
  base::TimeDelta next_timestamp_;
};

class RecordingServiceTest : public testing::Test,
                             public mojom::RecordingServiceClient {
 public:
  RecordingServiceTest() { CHECK(temp_dir_.CreateUniqueTempDir()); }
  RecordingServiceTest(const RecordingServiceTest&) = delete;
  RecordingServiceTest& operator=(const RecordingServiceTest&) = delete;
  ~RecordingServiceTest() override {
    service_api_.reset();
    task_environment_.FastForwardBy(base::TimeDelta());
  }

  FakeVideoCapturer& capturer() { return capturer_; }
  RecordingServiceTestApi* service_api() { return service_api_.get(); }

  void StartRecording(const std::string& file_name) {
    output_file_path_ = temp_dir_.GetPath().AppendASCII(file_name);
    service_api_ = std::make_unique<RecordingServiceTestApi>(
        service_remote_.BindNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    capturer_.set_on_started_closure(run_loop.QuitClosure());
    service_remote_->RecordFullscreen(
        client_receiver_.BindNewPipeAndPassRemote(),
        capturer_.BindNewPipeAndPassRemote(),
        /*microphone_stream_factory=*/mojo::NullRemote(),
        /*system_audio_stream_factory=*/mojo::NullRemote(),
        /*drive_fs_quota_delegate=*/mojo::NullRemote(), output_file_path_,
        viz::FrameSinkId(1, 1), kFrameSinkSize, /*device_scale_factor=*/1.f);
    run_loop.Run();
    task_environment_.FastForwardBy(base::TimeDelta());
  }

  void SendFrame(media::VideoPixelFormat pixel_format) {
    capturer_.SendFrame(pixel_format, kFrameSinkSize);
    task_environment_.FastForwardBy(base::TimeDelta());
  }

  void FinishRecordingAndExpectSuccess() {
    base::RunLoop run_loop;
    on_recording_ended_closure_ = run_loop.QuitClosure();
    service_remote_->StopRecording();
    run_loop.Run();

    ASSERT_TRUE(last_status_.has_value());
    EXPECT_EQ(mojom::RecordingStatus::kSuccess, *last_status_);
    std::optional<int64_t> file_size = base::GetFileSize(output_file_path_);
    ASSERT_TRUE(file_size.has_value());
    EXPECT_GT(*file_size, 0);
  }

  // mojom::RecordingServiceClient:
  void OnRecordingEnded(mojom::RecordingStatus status,
                        const gfx::ImageSkia& thumbnail) override {
    last_status_ = status;
    if (on_recording_ended_closure_) {
      std::move(on_recording_ended_closure_).Run();
    }
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  base::FilePath output_file_path_;
  mojo::Receiver<mojom::RecordingServiceClient> client_receiver_{this};
  base::OnceClosure on_recording_ended_closure_;
  std::optional<mojom::RecordingStatus> last_status_;
  FakeVideoCapturer capturer_;
  mojo::Remote<mojom::RecordingService> service_remote_;
  std::unique_ptr<RecordingServiceTestApi> service_api_;
};

TEST_F(RecordingServiceTest, GifRecordingPixelFormat) {
  StartRecording("test.gif");
  EXPECT_EQ(media::PIXEL_FORMAT_ARGB, capturer().requested_format());

  SendFrame(media::PIXEL_FORMAT_I420);
  EXPECT_TRUE(service_api()->GetVideoThumbnail().isNull());

  SendFrame(media::PIXEL_FORMAT_ARGB);
  EXPECT_FALSE(service_api()->GetVideoThumbnail().isNull());

  FinishRecordingAndExpectSuccess();
}

TEST_F(RecordingServiceTest, WebmRecordingPixelFormat) {
  StartRecording("test.webm");
  EXPECT_EQ(media::PIXEL_FORMAT_I420, capturer().requested_format());

  SendFrame(media::PIXEL_FORMAT_ARGB);
  EXPECT_TRUE(service_api()->GetVideoThumbnail().isNull());

  SendFrame(media::PIXEL_FORMAT_I420);
  EXPECT_FALSE(service_api()->GetVideoThumbnail().isNull());

  FinishRecordingAndExpectSuccess();
}

TEST(RgbVideoFrameTest, CopiesVisiblePixels) {
  constexpr gfx::Size kCodedSize{4, 3};
  std::vector<RgbColor> pixels(kCodedSize.GetArea());
  for (int i = 0; i < kCodedSize.GetArea(); ++i) {
    pixels[i] = RgbColor(i, i + 10, i + 20);
    pixels[i].ignored_a = 0xFF;
  }

  const gfx::Rect kVisibleRects[] = {gfx::Rect(kCodedSize),
                                     gfx::Rect(1, 1, 2, 2)};
  for (const gfx::Rect& visible_rect : kVisibleRects) {
    scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::WrapExternalData(
            media::PIXEL_FORMAT_ARGB, kCodedSize, visible_rect,
            visible_rect.size(), base::as_byte_span(pixels), base::TimeDelta());
    ASSERT_TRUE(frame);

    const RgbVideoFrame rgb_frame(*frame);
    ASSERT_EQ(rgb_frame.width(), visible_rect.width());
    ASSERT_EQ(rgb_frame.height(), visible_rect.height());
    for (int r = 0; r < rgb_frame.height(); ++r) {
      for (int c = 0; c < rgb_frame.width(); ++c) {
        const int src =
            (visible_rect.y() + r) * kCodedSize.width() + visible_rect.x() + c;
        EXPECT_EQ(rgb_frame.pixel_color(r, c), pixels[src]);
      }
    }
  }
}

TEST(RgbVideoFrameDeathTest, NonArgbFrame) {
  constexpr gfx::Size kSize{4, 2};
  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateZeroInitializedFrame(media::PIXEL_FORMAT_I420,
                                                    kSize, gfx::Rect(kSize),
                                                    kSize, base::TimeDelta());
  ASSERT_TRUE(frame);
  EXPECT_CHECK_DEATH({ RgbVideoFrame rgb_frame(*frame); });
}

}  // namespace

}  // namespace recording
