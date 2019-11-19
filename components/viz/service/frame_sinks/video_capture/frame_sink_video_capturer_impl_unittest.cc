// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_impl.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/service/frame_sinks/video_capture/frame_sink_video_capturer_manager.h"
#include "media/base/limits.h"
#include "media/base/video_util.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_space.h"
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
    const gfx::Vector2dF& root_scroll_offset) {
  double dsf, psf, rso_x, rso_y;
  bool valid = true;

  valid &= frame.metadata()->GetDouble(
      media::VideoFrameMetadata::DEVICE_SCALE_FACTOR, &dsf);
  valid &= frame.metadata()->GetDouble(
      media::VideoFrameMetadata::PAGE_SCALE_FACTOR, &psf);
  valid &= frame.metadata()->GetDouble(
      media::VideoFrameMetadata::ROOT_SCROLL_OFFSET_X, &rso_x);
  valid &= frame.metadata()->GetDouble(
      media::VideoFrameMetadata::ROOT_SCROLL_OFFSET_Y, &rso_y);

  return valid && dsf == device_scale_factor && psf == page_scale_factor &&
         gfx::Vector2dF(rso_x, rso_y) == root_scroll_offset;
}

// Dummy frame sink ID.
constexpr FrameSinkId kFrameSinkId = FrameSinkId(1, 1);

// The compositor frame interval.
constexpr base::TimeDelta kVsyncInterval =
    base::TimeDelta::FromSecondsD(1.0 / 60.0);

const struct SizeSet {
  // The size of the compositor frame sink's Surface.
  gfx::Size source_size;
  // The size of the VideoFrames produced by the capturer.
  gfx::Size capture_size;
  // The location of the letterboxed content within each VideoFrame. All pixels
  // outside of this region should be black.
  gfx::Rect expected_content_rect;
} kSizeSets[3] = {
    {gfx::Size(100, 100), gfx::Size(32, 18), gfx::Rect(6, 0, 18, 18)},
    {gfx::Size(64, 18), gfx::Size(32, 18), gfx::Rect(0, 4, 32, 8)},
    {gfx::Size(64, 18), gfx::Size(64, 18), gfx::Rect(0, 0, 64, 18)}};

constexpr float kDefaultDeviceScaleFactor = 1.f;
constexpr float kDefaultPageScaleFactor = 1.f;
constexpr gfx::Vector2dF kDefaultRootScrollOffset = gfx::Vector2dF(0, 0);

struct YUVColor {
  uint8_t y;
  uint8_t u;
  uint8_t v;
};

// Forces any pending Mojo method calls between the capturer and consumer to be
// made.
void PropagateMojoTasks() {
  base::RunLoop().RunUntilIdle();
}

class MockFrameSinkManager : public FrameSinkVideoCapturerManager {
 public:
  MOCK_METHOD1(FindCapturableFrameSink,
               CapturableFrameSink*(const FrameSinkId& frame_sink_id));
  MOCK_METHOD1(OnCapturerConnectionLost,
               void(FrameSinkVideoCapturerImpl* capturer));
};

class MockConsumer : public mojom::FrameSinkVideoConsumer {
 public:
  MockConsumer() {}

  MOCK_METHOD0(OnFrameCapturedMock, void());
  MOCK_METHOD0(OnStopped, void());

  int num_frames_received() const { return frames_.size(); }

  scoped_refptr<VideoFrame> TakeFrame(int i) { return std::move(frames_[i]); }

  void SendDoneNotification(int i) {
    std::move(done_callbacks_[i]).Run();
    PropagateMojoTasks();
  }

  mojo::PendingRemote<mojom::FrameSinkVideoConsumer> BindVideoConsumer() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  void OnFrameCaptured(
      base::ReadOnlySharedMemoryRegion data,
      media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& expected_content_rect,
      mojo::PendingRemote<mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks) final {
    ASSERT_TRUE(data.IsValid());
    const auto required_bytes_to_hold_planes =
        static_cast<uint32_t>(info->coded_size.GetArea() * 3 / 2);
    ASSERT_LE(required_bytes_to_hold_planes, data.GetSize());
    ASSERT_TRUE(info);

    mojo::Remote<mojom::FrameSinkVideoConsumerFrameCallbacks> callbacks_remote(
        std::move(callbacks));
    ASSERT_TRUE(callbacks_remote.get());

    // Map the shared memory buffer and re-constitute a VideoFrame instance
    // around it for analysis via TakeFrame().
    base::ReadOnlySharedMemoryMapping mapping = data.Map();
    ASSERT_TRUE(mapping.IsValid());
    ASSERT_LE(
        media::VideoFrame::AllocationSize(info->pixel_format, info->coded_size),
        mapping.size());
    scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::WrapExternalData(
            info->pixel_format, info->coded_size, info->visible_rect,
            info->visible_rect.size(),
            const_cast<uint8_t*>(static_cast<const uint8_t*>(mapping.memory())),
            mapping.size(), info->timestamp);
    ASSERT_TRUE(frame);
    frame->metadata()->MergeInternalValuesFrom(info->metadata);
    if (info->color_space.has_value())
      frame->set_color_space(info->color_space.value());

    frame->AddDestructionObserver(base::BindOnce(
        [](base::ReadOnlySharedMemoryMapping mapping) {}, std::move(mapping)));
    OnFrameCapturedMock();

    frames_.push_back(std::move(frame));
    done_callbacks_.push_back(
        base::BindOnce(&mojom::FrameSinkVideoConsumerFrameCallbacks::Done,
                       std::move(callbacks_remote)));
  }

  mojo::Receiver<mojom::FrameSinkVideoConsumer> receiver_{this};
  std::vector<scoped_refptr<VideoFrame>> frames_;
  std::vector<base::OnceClosure> done_callbacks_;
};

class SolidColorI420Result : public CopyOutputResult {
 public:
  SolidColorI420Result(const gfx::Rect rect, YUVColor color)
      : CopyOutputResult(CopyOutputResult::Format::I420_PLANES, rect),
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

  void AttachCaptureClient(Client* client) override {
    ASSERT_FALSE(client_);
    ASSERT_TRUE(client);
    client_ = client;
  }

  void DetachCaptureClient(Client* client) override {
    ASSERT_TRUE(client);
    ASSERT_EQ(client, client_);
    client_ = nullptr;
  }

  gfx::Size GetActiveFrameSize() override { return source_size(); }

  void RequestCopyOfOutput(
      const LocalSurfaceId& local_surface_id,
      std::unique_ptr<CopyOutputRequest> request) override {
    EXPECT_EQ(CopyOutputResult::Format::I420_PLANES, request->result_format());
    EXPECT_NE(base::UnguessableToken(), request->source());
    EXPECT_EQ(gfx::Rect(size_set_.source_size), request->area());
    EXPECT_EQ(gfx::Rect(size_set_.expected_content_rect.size()),
              request->result_selection());

    auto result = std::make_unique<SolidColorI420Result>(
        request->result_selection(), color_);
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

  void set_size_set(const SizeSet& size_set) { size_set_ = size_set; }

  void set_metadata(const CompositorFrameMetadata& metadata) {
    metadata_ = metadata.Clone();
  }

  void SetCopyOutputColor(YUVColor color) { color_ = color; }

  int num_copy_results() const { return results_.size(); }

  void SendCopyOutputResult(int offset) {
    auto it = results_.begin() + offset;
    std::move(*it).Run();
    PropagateMojoTasks();
  }

 private:
  CapturableFrameSink::Client* client_ = nullptr;
  YUVColor color_ = {0xde, 0xad, 0xbf};
  SizeSet size_set_;
  CompositorFrameMetadata metadata_;

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
    if (forced_capture_size_.has_value())
      return forced_capture_size_.value();
    return media::VideoCaptureOracle::capture_size();
  }

  void set_forced_capture_size(base::Optional<gfx::Size> size) {
    forced_capture_size_ = size;
  }

 private:
  bool return_false_on_complete_capture_;
  base::Optional<gfx::Size> forced_capture_size_;
};

// Matcher that returns true if the content region of a letterboxed VideoFrame
// is filled with the given color, and black everywhere else.
MATCHER_P2(IsLetterboxedFrame, color, content_rect, "") {
  if (!arg) {
    return false;
  }

  const VideoFrame& frame = *arg;
  const gfx::Rect kContentRect = content_rect;
  const auto IsLetterboxedPlane = [&frame, kContentRect](int plane,
                                                         uint8_t color) {
    gfx::Rect content_rect_copy = kContentRect;
    if (plane != VideoFrame::kYPlane) {
      content_rect_copy = gfx::Rect(
          content_rect_copy.x() / 2, content_rect_copy.y() / 2,
          content_rect_copy.width() / 2, content_rect_copy.height() / 2);
    }
    for (int row = 0; row < frame.rows(plane); ++row) {
      const uint8_t* p = frame.visible_data(plane) + row * frame.stride(plane);
      for (int col = 0; col < frame.row_bytes(plane); ++col) {
        if (content_rect_copy.Contains(gfx::Point(col, row))) {
          if (p[col] != color) {
            return false;
          }
        } else {  // Letterbox border around content.
          if (plane == VideoFrame::kYPlane && p[col] != 0x00) {
            return false;
          }
        }
      }
    }
    return true;
  };

  return IsLetterboxedPlane(VideoFrame::kYPlane, color.y) &&
         IsLetterboxedPlane(VideoFrame::kUPlane, color.u) &&
         IsLetterboxedPlane(VideoFrame::kVPlane, color.v);
}

}  // namespace

class FrameSinkVideoCapturerTest : public testing::Test {
 public:
  FrameSinkVideoCapturerTest() : size_set_(kSizeSets[0]) {
    auto oracle = std::make_unique<InstrumentedVideoCaptureOracle>(
        true /* enable_auto_throttling */);
    oracle_ = oracle.get();
    capturer_ = std::make_unique<FrameSinkVideoCapturerImpl>(
        &frame_sink_manager_, mojo::NullReceiver(), std::move(oracle));
  }

  void SetUp() override {
    // Override the capturer's TickClock with a virtual clock managed by a
    // manually-driven task runner.
    task_runner_ = new base::TestMockTimeTaskRunner(
        base::Time::Now(), base::TimeTicks() + base::TimeDelta::FromSeconds(1),
        base::TestMockTimeTaskRunner::Type::kStandalone);
    start_time_ = task_runner_->NowTicks();
    capturer_->clock_ = task_runner_->GetMockTickClock();

    // Replace the retry timer with one that uses this test's fake clock and
    // task runner.
    capturer_->refresh_frame_retry_timer_.emplace(
        task_runner_->GetMockTickClock());
    capturer_->refresh_frame_retry_timer_->SetTaskRunner(task_runner_);

    // Before setting the format, ensure the defaults are in-place. Then, for
    // these tests, set a specific format and color space.
    ASSERT_EQ(FrameSinkVideoCapturerImpl::kDefaultPixelFormat,
              capturer_->pixel_format_);
    ASSERT_EQ(FrameSinkVideoCapturerImpl::kDefaultColorSpace,
              capturer_->color_space_);
    capturer_->SetFormat(media::PIXEL_FORMAT_I420,
                         gfx::ColorSpace::CreateREC709());
    ASSERT_EQ(media::PIXEL_FORMAT_I420, capturer_->pixel_format_);
    ASSERT_EQ(gfx::ColorSpace::CreateREC709(), capturer_->color_space_);

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
    capturer_->Start(consumer->BindVideoConsumer());
    PropagateMojoTasks();
  }

  void StopCapture() {
    capturer_->Stop();
    PropagateMojoTasks();
  }

  base::TimeTicks GetNextVsync() const {
    const auto now = task_runner_->NowTicks();
    const auto num_vsyncs_elapsed = (now - start_time_) / kVsyncInterval;
    return start_time_ + (num_vsyncs_elapsed + 1) * kVsyncInterval;
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

  const SizeSet& size_set() { return size_set_; }

  void NotifyFrameDamaged(
      gfx::Rect damage_rect,
      float device_scale_factor = kDefaultDeviceScaleFactor,
      float page_scale_factor = kDefaultPageScaleFactor,
      gfx::Vector2dF root_scroll_offset = kDefaultRootScrollOffset) {
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

 protected:
  SizeSet size_set_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::TimeTicks start_time_;
  MockFrameSinkManager frame_sink_manager_;
  FakeCapturableFrameSink frame_sink_;
  std::unique_ptr<FrameSinkVideoCapturerImpl> capturer_;
  InstrumentedVideoCaptureOracle* oracle_;
};

// Tests that the capturer attaches to a frame sink immediately, in the case
// where the frame sink was already known by the manager.
TEST_F(FrameSinkVideoCapturerTest, ResolvesTargetImmediately) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillRepeatedly(Return(&frame_sink_));

  EXPECT_EQ(FrameSinkId(), capturer_->requested_target());
  capturer_->ChangeTarget(kFrameSinkId);
  EXPECT_EQ(kFrameSinkId, capturer_->requested_target());
  EXPECT_EQ(capturer_.get(), frame_sink_.attached_client());
}

// Tests that the capturer attaches to a frame sink later, in the case where the
// frame sink becomes known to the manager at some later point.
TEST_F(FrameSinkVideoCapturerTest, ResolvesTargetLater) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillRepeatedly(Return(nullptr));

  EXPECT_EQ(FrameSinkId(), capturer_->requested_target());
  capturer_->ChangeTarget(kFrameSinkId);
  EXPECT_EQ(kFrameSinkId, capturer_->requested_target());
  EXPECT_EQ(nullptr, frame_sink_.attached_client());

  capturer_->SetResolvedTarget(&frame_sink_);
  EXPECT_EQ(capturer_.get(), frame_sink_.attached_client());
}

// Tests that no initial frame is sent after Start() is called until after the
// target has been resolved.
TEST_F(FrameSinkVideoCapturerTest, PostponesCaptureWithoutATarget) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
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
  capturer_->ChangeTarget(kFrameSinkId);
  EXPECT_EQ(1, frame_sink_.num_copy_results());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  StopCapture();
  EXPECT_FALSE(IsRefreshRetryTimerRunning());
}

// An end-to-end pipeline test where compositor updates trigger the capturer to
// make copy requests, and a stream of video frames is delivered to the
// consumer.
TEST_F(FrameSinkVideoCapturerTest, CapturesCompositedFrames) {
  frame_sink_.SetCopyOutputColor(YUVColor{0x80, 0x80, 0x80});
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillRepeatedly(Return(&frame_sink_));

  capturer_->ChangeTarget(kFrameSinkId);
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
              IsLetterboxedFrame(YUVColor{0x80, 0x80, 0x80},
                                 size_set().expected_content_rect));
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
    const YUVColor color = {i << 4, (i << 4) + 0x10, (i << 4) + 0x20};
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
    EXPECT_THAT(frame,
                IsLetterboxedFrame(color, size_set().expected_content_rect));
    EXPECT_EQ(size_set().capture_size, frame->coded_size());
    EXPECT_EQ(gfx::Rect(size_set().capture_size), frame->visible_rect());
    EXPECT_LT(last_timestamp, frame->timestamp());
    last_timestamp = frame->timestamp();
    const VideoFrameMetadata* metadata = frame->metadata();
    base::TimeTicks capture_begin_time;
    EXPECT_TRUE(metadata->GetTimeTicks(VideoFrameMetadata::CAPTURE_BEGIN_TIME,
                                       &capture_begin_time));
    EXPECT_EQ(expected_capture_begin_time, capture_begin_time);
    base::TimeTicks capture_end_time;
    EXPECT_TRUE(metadata->GetTimeTicks(VideoFrameMetadata::CAPTURE_END_TIME,
                                       &capture_end_time));
    EXPECT_EQ(expected_capture_end_time, capture_end_time);
    EXPECT_EQ(gfx::ColorSpace::CreateREC709(), frame->ColorSpace());
    EXPECT_TRUE(metadata->HasKey(VideoFrameMetadata::FRAME_DURATION));
    // FRAME_DURATION is an estimate computed by the VideoCaptureOracle, so it
    // its exact value is not being checked here.
    double frame_rate = 0.0;
    EXPECT_TRUE(
        metadata->GetDouble(VideoFrameMetadata::FRAME_RATE, &frame_rate));
    EXPECT_NEAR(media::limits::kMaxFramesPerSecond, frame_rate, 0.001);
    base::TimeTicks reference_time;
    EXPECT_TRUE(metadata->GetTimeTicks(VideoFrameMetadata::REFERENCE_TIME,
                                       &reference_time));
    EXPECT_EQ(expected_reference_time, reference_time);

    // Notify the capturer that the consumer is done with the frame.
    consumer.SendDoneNotification(i);

    if (HasFailure()) {
      break;
    }
  }

  StopCapture();
}

// Tests that frame capturing halts when too many frames are in-flight, whether
// that is because there are too many copy requests in-flight or because the
// consumer has not finished consuming frames fast enough.
TEST_F(FrameSinkVideoCapturerTest, HaltsWhenPipelineIsFull) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillRepeatedly(Return(&frame_sink_));

  capturer_->ChangeTarget(kFrameSinkId);

  NiceMock<MockConsumer> consumer;
  StartCapture(&consumer);
  // With the start, an immediate refresh occurred.
  const int num_refresh_frames = 1;
  ASSERT_EQ(num_refresh_frames, frame_sink_.num_copy_results());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  // Saturate the pipeline with CopyOutputRequests that have not yet executed.
  int num_frames = FrameSinkVideoCapturerImpl::kDesignLimitMaxFrames;
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

  // Notify the capturer that the first frame has been consumed. Then, with
  // another compositor update, the capturer should issue another new copy
  // request. The refresh timer should no longer be running because the next
  // capture will satisfy the need to send updated content to the consumer.
  EXPECT_TRUE(consumer.TakeFrame(0));
  consumer.SendDoneNotification(0);
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

  // Complete all pending copy requests. Another compositor update should not
  // cause any new copy requests to be issued because all frames are being
  // delivered/consumed.
  for (int i = 1; i < frame_sink_.num_copy_results(); ++i) {
    SCOPED_TRACE(testing::Message() << "frame #" << i);
    frame_sink_.SendCopyOutputResult(i);
  }
  ASSERT_EQ(frame_sink_.num_copy_results(), consumer.num_frames_received());
  AdvanceClockToNextVsync();
  NotifyFrameDamaged(gfx::Rect(size_set().source_size));
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());
  EXPECT_TRUE(IsRefreshRetryTimerRunning());

  // Notify the capturer that all frames have been consumed. Finally, with
  // another compositor update, capture should resume.
  for (int i = 1; i < consumer.num_frames_received(); ++i) {
    SCOPED_TRACE(testing::Message() << "frame #" << i);
    EXPECT_TRUE(consumer.TakeFrame(i));
    consumer.SendDoneNotification(i);
  }
  AdvanceClockToNextVsync();
  NotifyFrameDamaged(gfx::Rect(size_set().source_size));
  ++num_frames;
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());
  frame_sink_.SendCopyOutputResult(frame_sink_.num_copy_results() - 1);
  ASSERT_EQ(frame_sink_.num_copy_results(), consumer.num_frames_received());
  EXPECT_FALSE(IsRefreshRetryTimerRunning());

  StopCapture();
}

// Tests that copy requests completed out-of-order are accounted for by the
// capturer, with results delivered to the consumer in-order.
TEST_F(FrameSinkVideoCapturerTest, DeliversFramesInOrder) {
  std::vector<YUVColor> colors;
  colors.push_back(YUVColor{0x00, 0x80, 0x80});
  frame_sink_.SetCopyOutputColor(colors.back());
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillRepeatedly(Return(&frame_sink_));

  capturer_->ChangeTarget(kFrameSinkId);

  NiceMock<MockConsumer> consumer;
  StartCapture(&consumer);

  // Simulate five compositor updates. Each composited frame has its content
  // region set to a different color to check that the video frames are being
  // delivered in-order.
  constexpr int num_refresh_frames = 1;
  constexpr int num_frames = 5;
  ASSERT_EQ(num_refresh_frames, frame_sink_.num_copy_results());
  for (int i = num_refresh_frames; i < num_frames; ++i) {
    colors.push_back(YUVColor{static_cast<uint8_t>(i << 4),
                              static_cast<uint8_t>((i << 4) + 0x10),
                              static_cast<uint8_t>((i << 4) + 0x20)});
    frame_sink_.SetCopyOutputColor(colors.back());
    AdvanceClockToNextVsync();
    NotifyFrameDamaged(gfx::Rect(size_set().source_size));
  }
  ASSERT_EQ(num_frames, frame_sink_.num_copy_results());

  // Complete the copy requests out-of-order. Check that frames are not
  // delivered until they can all be delivered in-order, and that the content of
  // each video frame is correct.
  frame_sink_.SendCopyOutputResult(0);
  ASSERT_EQ(1, consumer.num_frames_received());
  EXPECT_THAT(consumer.TakeFrame(0),
              IsLetterboxedFrame(colors[0], size_set().expected_content_rect));
  frame_sink_.SendCopyOutputResult(2);
  ASSERT_EQ(1, consumer.num_frames_received());  // Waiting for frame 1.
  frame_sink_.SendCopyOutputResult(3);
  ASSERT_EQ(1, consumer.num_frames_received());  // Still waiting for frame 1.
  frame_sink_.SendCopyOutputResult(1);
  ASSERT_EQ(4, consumer.num_frames_received());  // Sent frames 1, 2, and 3.
  EXPECT_THAT(consumer.TakeFrame(1),
              IsLetterboxedFrame(colors[1], size_set().expected_content_rect));
  EXPECT_THAT(consumer.TakeFrame(2),
              IsLetterboxedFrame(colors[2], size_set().expected_content_rect));
  EXPECT_THAT(consumer.TakeFrame(3),
              IsLetterboxedFrame(colors[3], size_set().expected_content_rect));
  frame_sink_.SendCopyOutputResult(4);
  ASSERT_EQ(5, consumer.num_frames_received());
  EXPECT_THAT(consumer.TakeFrame(4),
              IsLetterboxedFrame(colors[4], size_set().expected_content_rect));

  StopCapture();
}

// Tests that in-flight copy requests are canceled when the capturer is
// stopped. When it is started again with a new consumer, only the results from
// newer copy requests should appear in video frames delivered to the consumer.
TEST_F(FrameSinkVideoCapturerTest, CancelsInFlightCapturesOnStop) {
  const YUVColor color1 = {0xaa, 0xbb, 0xcc};
  frame_sink_.SetCopyOutputColor(color1);
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillRepeatedly(Return(&frame_sink_));

  capturer_->ChangeTarget(kFrameSinkId);

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
                IsLetterboxedFrame(color1, size_set().expected_content_rect));
  }

  // Stopping capture should cancel the remaning copy requests.
  StopCapture();

  // Change the content color and start capturing to the second consumer.
  const YUVColor color2 = {0xdd, 0xee, 0xff};
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
                IsLetterboxedFrame(color2, size_set().expected_content_rect));
  }

  StopCapture();
}

// Tests that refresh requests ultimately result in a frame being delivered to
// the consumer.
TEST_F(FrameSinkVideoCapturerTest, EventuallySendsARefreshFrame) {
  frame_sink_.SetCopyOutputColor(YUVColor{0x80, 0x80, 0x80});
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillRepeatedly(Return(&frame_sink_));

  capturer_->ChangeTarget(kFrameSinkId);

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

// Tests that CompositorFrameMetadata variables (|device_scale_factor|,
// |page_scale_factor| and |root_scroll_offset|) are sent along with each frame,
// and refreshes cause variables of the cached CompositorFrameMetadata
// (|last_frame_metadata|) to be used.
TEST_F(FrameSinkVideoCapturerTest, CompositorFrameMetadataReachesConsumer) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillRepeatedly(Return(&frame_sink_));
  capturer_->ChangeTarget(kFrameSinkId);

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
  const gfx::Vector2dF kNewRootScrollOffset = gfx::Vector2dF(100, 200);

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
TEST_F(FrameSinkVideoCapturerTest, DeliversUpdateRectAndCaptureCounter) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillRepeatedly(Return(&frame_sink_));
  capturer_->ChangeTarget(kFrameSinkId);

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
    gfx::Rect received_update_rect;
    int received_capture_counter = 0;
    ASSERT_TRUE(received_frame->metadata()->GetInteger(
        media::VideoFrameMetadata::CAPTURE_COUNTER, &received_capture_counter));
    ASSERT_TRUE(received_frame->metadata()->GetRect(
        media::VideoFrameMetadata::CAPTURE_UPDATE_RECT, &received_update_rect));
    EXPECT_EQ(gfx::Rect(size_set().capture_size), received_update_rect);
    previous_capture_counter_received = received_capture_counter;
  }
  consumer.SendDoneNotification(cur_frame_index);

  const gfx::Rect kSourceDamageRect = gfx::Rect(3, 7, 60, 45);
  gfx::Rect expected_frame_update_rect = copy_output::ComputeResultRect(
      kSourceDamageRect,
      gfx::Vector2d(size_set().source_size.width(),
                    size_set().source_size.height()),
      gfx::Vector2d(size_set().expected_content_rect.width(),
                    size_set().expected_content_rect.height()));
  expected_frame_update_rect.Offset(
      size_set().expected_content_rect.OffsetFromOrigin());
  EXPECT_FALSE(AlignsWithI420SubsamplingBoundaries(expected_frame_update_rect));
  expected_frame_update_rect =
      ExpandRectToI420SubsampleBoundaries(expected_frame_update_rect);
  EXPECT_TRUE(AlignsWithI420SubsamplingBoundaries(expected_frame_update_rect));

  // Notify frame damage with custom damage rect, and expect that the refresh
  // frame is delivered to the consumer with a corresponding |update_rect|.
  AdvanceClockToNextVsync();
  NotifyFrameDamaged(kSourceDamageRect);
  frame_sink_.SendCopyOutputResult(++cur_capture_index);
  ++expected_frames_count;
  EXPECT_EQ(expected_frames_count, consumer.num_frames_received());
  {
    auto received_frame = consumer.TakeFrame(++cur_frame_index);
    int received_capture_counter = 0;
    gfx::Rect received_update_rect;
    ASSERT_TRUE(received_frame->metadata()->GetInteger(
        media::VideoFrameMetadata::CAPTURE_COUNTER, &received_capture_counter));
    ASSERT_TRUE(received_frame->metadata()->GetRect(
        media::VideoFrameMetadata::CAPTURE_UPDATE_RECT, &received_update_rect));
    EXPECT_EQ(expected_frame_update_rect, received_update_rect);
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
    int received_capture_counter = 0;
    gfx::Rect received_update_rect;
    ASSERT_TRUE(received_frame->metadata()->GetInteger(
        media::VideoFrameMetadata::CAPTURE_COUNTER, &received_capture_counter));
    ASSERT_TRUE(received_frame->metadata()->GetRect(
        media::VideoFrameMetadata::CAPTURE_UPDATE_RECT, &received_update_rect));
    EXPECT_TRUE(received_update_rect.IsEmpty());
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
    int received_capture_counter = 0;
    gfx::Rect received_update_rect;
    ASSERT_TRUE(received_frame->metadata()->GetInteger(
        media::VideoFrameMetadata::CAPTURE_COUNTER, &received_capture_counter));
    ASSERT_TRUE(received_frame->metadata()->GetRect(
        media::VideoFrameMetadata::CAPTURE_UPDATE_RECT, &received_update_rect));
    EXPECT_EQ(gfx::Rect(size_set().capture_size), received_update_rect);
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
    int received_capture_counter = 0;
    gfx::Rect received_update_rect;
    ASSERT_TRUE(received_frame->metadata()->GetInteger(
        media::VideoFrameMetadata::CAPTURE_COUNTER, &received_capture_counter));
    ASSERT_TRUE(received_frame->metadata()->GetRect(
        media::VideoFrameMetadata::CAPTURE_UPDATE_RECT, &received_update_rect));
    EXPECT_EQ(gfx::Rect(size_set().capture_size), received_update_rect);
    EXPECT_EQ(previous_capture_counter_received + 1, received_capture_counter);
    previous_capture_counter_received = received_capture_counter;
  }

  StopCapture();
}

// Tests that when captured frames being dropped before delivery, the
// CAPTURE_COUNTER metadata value sent to the consumer reflects the frame drops
// indicating that CAPTURE_UPDATE_RECT must be ignored.
TEST_F(FrameSinkVideoCapturerTest, CaptureCounterSkipsWhenFramesAreDropped) {
  EXPECT_CALL(frame_sink_manager_, FindCapturableFrameSink(kFrameSinkId))
      .WillRepeatedly(Return(&frame_sink_));
  capturer_->ChangeTarget(kFrameSinkId);

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
    int received_capture_counter = 0;
    gfx::Rect received_update_rect;
    ASSERT_TRUE(received_frame->metadata()->GetInteger(
        media::VideoFrameMetadata::CAPTURE_COUNTER, &received_capture_counter));
    ASSERT_TRUE(received_frame->metadata()->GetRect(
        media::VideoFrameMetadata::CAPTURE_UPDATE_RECT, &received_update_rect));
    EXPECT_EQ(gfx::Rect(size_set().capture_size), received_update_rect);
    previous_capture_counter_received = received_capture_counter;
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
    int received_capture_counter = 0;
    ASSERT_TRUE(received_frame->metadata()->GetInteger(
        media::VideoFrameMetadata::CAPTURE_COUNTER, &received_capture_counter));
    EXPECT_NE(previous_capture_counter_received + 1, received_capture_counter);
  }
  StopCapture();
}

}  // namespace viz
