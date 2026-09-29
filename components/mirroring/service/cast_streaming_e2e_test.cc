// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "components/mirroring/mojom/resource_provider.mojom.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "components/mirroring/service/fake_socket_factory.h"
#include "components/mirroring/service/fake_video_capture_host.h"
#include "components/mirroring/service/openscreen_session_host.h"
#include "components/mirroring/service/test_receiver_session_helpers.h"
#include "components/openscreen_platform/task_runner.h"
#include "media/base/media_switches.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/cast/cast_config.h"
#include "media/cast/encoding/encoding_support.h"
#include "media/cast/test/fake_openscreen_clock.h"
#include "media/cast/test/utility/video_utility.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"
#include "third_party/openscreen/src/cast/streaming/public/constants.h"
#include "third_party/openscreen/src/cast/streaming/public/environment.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver_constraints.h"
#include "third_party/openscreen/src/cast/streaming/public/receiver_session.h"
#include "third_party/openscreen/src/platform/base/ip_address.h"
#include "ui/gfx/codec/png_codec.h"

using ::testing::NiceMock;

namespace mirroring {

namespace {

constexpr char kSenderId[] = "sender-123";
constexpr char kReceiverId[] = "receiver-456";
constexpr int kMaxTolerance = 10;

// Converts a VideoFrame to a base64-encoded PNG data string for diagnostic
// error logging on pixel test mismatches.
std::string VideoFrameToBase64PNG(const media::VideoFrame& frame) {
  scoped_refptr<media::VideoFrame> i420_frame;
  if (frame.format() == media::PIXEL_FORMAT_I420) {
    i420_frame = const_cast<media::VideoFrame*>(&frame);
  } else if (frame.format() == media::PIXEL_FORMAT_NV12) {
    i420_frame = media::VideoFrame::CreateFrame(
        media::PIXEL_FORMAT_I420, frame.coded_size(), frame.visible_rect(),
        frame.natural_size(), frame.timestamp());
    libyuv::NV12ToI420(
        frame.data(media::VideoFrame::Plane::kY),
        frame.stride(media::VideoFrame::Plane::kY),
        frame.data(media::VideoFrame::Plane::kUV),
        frame.stride(media::VideoFrame::Plane::kUV),
        i420_frame->GetWritableVisiblePlaneData(media::VideoFrame::Plane::kY)
            .data(),
        i420_frame->stride(media::VideoFrame::Plane::kY),
        i420_frame->GetWritableVisiblePlaneData(media::VideoFrame::Plane::kU)
            .data(),
        i420_frame->stride(media::VideoFrame::Plane::kU),
        i420_frame->GetWritableVisiblePlaneData(media::VideoFrame::Plane::kV)
            .data(),
        i420_frame->stride(media::VideoFrame::Plane::kV),
        frame.visible_rect().width(), frame.visible_rect().height());
  } else {
    return "<unsupported format for PNG dump>";
  }

  const gfx::Size size = i420_frame->visible_rect().size();
  std::vector<uint8_t> rgba_data(size.width() * size.height() * 4);
  libyuv::I420ToRGBA(i420_frame->data(media::VideoFrame::Plane::kY),
                     i420_frame->stride(media::VideoFrame::Plane::kY),
                     i420_frame->data(media::VideoFrame::Plane::kU),
                     i420_frame->stride(media::VideoFrame::Plane::kU),
                     i420_frame->data(media::VideoFrame::Plane::kV),
                     i420_frame->stride(media::VideoFrame::Plane::kV),
                     rgba_data.data(), size.width() * 4, size.width(),
                     size.height());

  std::optional<std::vector<uint8_t>> png_data = gfx::PNGCodec::Encode(
      rgba_data.data(), gfx::PNGCodec::FORMAT_RGBA, size, size.width() * 4,
      /*discard_transparency=*/true, {});
  if (!png_data) {
    return "<failed to encode PNG>";
  }
  return base::Base64Encode(*png_data);
}

}  // namespace

class CastStreamingEndToEndTest : public ::testing::Test,
                                  public mojom::ResourceProvider,
                                  public mojom::SessionObserver {
 public:
  CastStreamingEndToEndTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
        resource_provider_receiver_(
            static_cast<mojom::ResourceProvider*>(this)),
        session_observer_receiver_(static_cast<mojom::SessionObserver*>(this)) {
    media::cast::FakeOpenscreenClock::SetTickClock(
        task_environment_.GetMockTickClock());
    media::cast::encoding_support::ClearHardwareCodecDenyListForTesting();
  }

  ~CastStreamingEndToEndTest() override {
    TearDown();
    media::cast::FakeOpenscreenClock::ClearTickClock();
    media::cast::encoding_support::ClearHardwareCodecDenyListForTesting();
  }

  void TearDown() override {
    if (consumer_) {
      consumer_->SetReceiver(nullptr, nullptr);
    }
    receiver_client_.set_consumer(nullptr, nullptr);
    if (receiver_session_) {
      receiver_session_.reset();
    }
    consumer_.reset();
    if (session_host_) {
      base::RunLoop session_host_deleted_loop;
      session_host_deleted_cb_ = session_host_deleted_loop.QuitClosure();
      session_host_.reset();
      session_host_deleted_loop.Run();
    }
    receiver_message_port_.reset();
    receiver_env_.reset();
    socket_factory_.reset();
    video_host_.reset();
  }

  // mojom::ResourceProvider implementation.
  void BindGpu(mojo::PendingReceiver<viz::mojom::Gpu> receiver) override {}
  void GetVideoCaptureHost(
      mojo::PendingReceiver<media::mojom::VideoCaptureHost> receiver) override {
    video_host_ =
        std::make_unique<NiceMock<FakeVideoCaptureHost>>(std::move(receiver));
    if (video_host_started_cb_) {
      video_host_->set_on_started_callback(std::move(video_host_started_cb_));
    }
  }
  void GetVideoEncoderMetricsProvider(
      mojo::PendingReceiver<media::mojom::VideoEncoderMetricsProvider> receiver)
      override {}
  void GetSocketFactory(
      mojo::PendingReceiver<network::mojom::SocketFactory> receiver) override {
    socket_factory_ =
        std::make_unique<NiceMock<MockSocketFactory>>(std::move(receiver));
    socket_factory_->set_udp_socket_created_callback(
        base::BindRepeating(&CastStreamingEndToEndTest::OnUdpSocketCreated,
                            base::Unretained(this)));
  }
  void OnUdpSocketCreated(MockUdpSocket* socket) {
    socket->set_packet_callback(base::BindRepeating(
        &CastStreamingEndToEndTest::OnSenderUdpPacket, base::Unretained(this)));
  }
  void CreateAudioStream(
      mojo::PendingRemote<mojom::AudioStreamCreatorClient> client,
      const media::AudioParameters& params,
      uint32_t total_segments) override {}
  void ConnectToRemotingSource(
      mojo::PendingRemote<media::mojom::Remoter> remoter,
      mojo::PendingReceiver<media::mojom::RemotingSource> receiver) override {}

  // mojom::SessionObserver implementation.
  void OnError(mojom::SessionError error) override {
    ADD_FAILURE() << "Unexpected SessionError: " << static_cast<int>(error);
  }
  void DidStart() override {
    if (did_start_cb_) {
      std::move(did_start_cb_).Run();
    }
  }
  void DidStop() override {}
  void LogInfoMessage(const std::string& message) override {
    DVLOG(1) << "[Mirroring Info] " << message;
  }
  void LogErrorMessage(const std::string& message) override {
    LOG(ERROR) << "[Mirroring Error] " << message;
  }
  void OnSourceChanged() override {}
  // Unused in mirroring tests; remoting is covered separately.
  void OnRemotingStateChanged(bool is_remoting) override {}

 protected:
  void SetUpStreamingSession(openscreen::cast::VideoCodec codec) {
    receiver_env_ = std::make_unique<TestReceiverEnvironment>(
        &media::cast::FakeOpenscreenClock::now, task_runner_);

    receiver_env_->set_packet_send_cb(
        base::BindRepeating(&CastStreamingEndToEndTest::OnReceiverRtcpPacket,
                            base::Unretained(this)));

    mojo::PendingRemote<mojom::CastMessageChannel> sender_to_receiver_remote;
    mojo::PendingReceiver<mojom::CastMessageChannel>
        sender_to_receiver_receiver =
            sender_to_receiver_remote.InitWithNewPipeAndPassReceiver();

    mojo::PendingRemote<mojom::CastMessageChannel> receiver_to_sender_remote;
    mojo::PendingReceiver<mojom::CastMessageChannel>
        receiver_to_sender_receiver =
            receiver_to_sender_remote.InitWithNewPipeAndPassReceiver();

    receiver_message_port_ = std::make_unique<TestReceiverMessagePort>(
        std::move(receiver_to_sender_remote),
        std::move(sender_to_receiver_receiver));

    openscreen::cast::ReceiverConstraints constraints;
    constraints.video_codecs = {codec};
    constraints.audio_codecs = {};

    receiver_session_ = std::make_unique<openscreen::cast::ReceiverSession>(
        receiver_client_, *receiver_env_, *receiver_message_port_,
        std::move(constraints));

    const media::VideoCodec media_codec =
        (codec == openscreen::cast::VideoCodec::kVp9) ? media::VideoCodec::kVP9
                                                      : media::VideoCodec::kVP8;
    consumer_ = std::make_unique<ReceiverConsumer>(media_codec);

    mojom::SessionParametersPtr session_params =
        mojom::SessionParameters::New();
    session_params->type = mojom::SessionType::VIDEO_ONLY;
    session_params->receiver_address = net::IPAddress::IPv4Localhost();
    session_params->receiver_friendly_name = "Cast Receiver Test";
    session_params->source_id = kSenderId;
    session_params->destination_id = kReceiverId;

    mojo::PendingRemote<mojom::ResourceProvider> resource_provider_remote;
    mojo::PendingRemote<mojom::SessionObserver> session_observer_remote;
    resource_provider_receiver_.Bind(
        resource_provider_remote.InitWithNewPipeAndPassReceiver());
    session_observer_receiver_.Bind(
        session_observer_remote.InitWithNewPipeAndPassReceiver());

    base::RunLoop session_start_loop;
    did_start_cb_ = session_start_loop.QuitClosure();

    base::RunLoop receiver_negotiated_loop;
    receiver_client_.set_on_negotiated(receiver_negotiated_loop.QuitClosure());
    receiver_client_.set_consumer(consumer_.get(), receiver_env_.get());

    session_host_ = std::make_unique<OpenscreenSessionHost>(
        std::move(session_params), gfx::Size(1920, 1080),
        std::move(session_observer_remote), std::move(resource_provider_remote),
        std::move(sender_to_receiver_remote),
        std::move(receiver_to_sender_receiver), nullptr,
        base::BindOnce(&CastStreamingEndToEndTest::OnSessionHostDeleted,
                       base::Unretained(this)));

    base::RunLoop video_host_start_loop;
    video_host_started_cb_ = video_host_start_loop.QuitClosure();

    session_host_->AsyncInitialize(base::DoNothing());

    receiver_negotiated_loop.Run();
    session_start_loop.Run();
    video_host_start_loop.Run();

    ASSERT_TRUE(receiver_client_.configured_receivers().video_receiver);
  }

  void OnSenderUdpPacket(const media::cast::Packet& packet) {
    if (drop_outgoing_packets_) {
      return;
    }
    if (receiver_env_) {
      receiver_env_->DeliverPacket(
          openscreen::IPEndpoint{openscreen::IPAddress{127, 0, 0, 1}, 5678},
          packet);
    }
  }

  void OnReceiverRtcpPacket(const std::vector<uint8_t>& packet) {
    if (socket_factory_ && socket_factory_->udp_socket()) {
      socket_factory_->udp_socket()->OnReceivedPacket(packet);
    }
  }

  void OnSessionHostDeleted() {
    if (session_host_deleted_cb_) {
      std::move(session_host_deleted_cb_).Run();
    }
  }

  void VerifyPixelFidelity(const media::VideoFrame& decoded,
                           media::VideoPixelFormat src_format,
                           const gfx::Size& frame_size,
                           int start_value) {
    EXPECT_EQ(decoded.format(), media::PIXEL_FORMAT_I420);
    EXPECT_EQ(decoded.visible_rect().size(), frame_size);

    scoped_refptr<media::VideoFrame> expected_frame =
        media::VideoFrame::CreateFrame(media::PIXEL_FORMAT_I420, frame_size,
                                       gfx::Rect(frame_size), frame_size,
                                       base::TimeDelta());
    if (src_format == media::PIXEL_FORMAT_NV12) {
      scoped_refptr<media::VideoFrame> nv12_frame =
          media::VideoFrame::CreateFrame(media::PIXEL_FORMAT_NV12, frame_size,
                                         gfx::Rect(frame_size), frame_size,
                                         base::TimeDelta());
      media::cast::PopulateVideoFrame(nv12_frame.get(), start_value);
      libyuv::NV12ToI420(
          nv12_frame->data(media::VideoFrame::Plane::kY),
          nv12_frame->stride(media::VideoFrame::Plane::kY),
          nv12_frame->data(media::VideoFrame::Plane::kUV),
          nv12_frame->stride(media::VideoFrame::Plane::kUV),
          expected_frame
              ->GetWritableVisiblePlaneData(media::VideoFrame::Plane::kY)
              .data(),
          expected_frame->stride(media::VideoFrame::Plane::kY),
          expected_frame
              ->GetWritableVisiblePlaneData(media::VideoFrame::Plane::kU)
              .data(),
          expected_frame->stride(media::VideoFrame::Plane::kU),
          expected_frame
              ->GetWritableVisiblePlaneData(media::VideoFrame::Plane::kV)
              .data(),
          expected_frame->stride(media::VideoFrame::Plane::kV),
          frame_size.width(), frame_size.height());
    } else {
      media::cast::PopulateVideoFrame(expected_frame.get(), start_value);
    }

    const int diff_pixels =
        media::CountDifferentPixels(decoded, *expected_frame, kMaxTolerance);
    const int max_allowed_diff_pixels = frame_size.width() * 4;
    if (diff_pixels > max_allowed_diff_pixels) {
      LOG(ERROR) << "Pixel mismatch failure! Different pixels: " << diff_pixels
                 << " (max allowed: " << max_allowed_diff_pixels
                 << ", tolerance: " << kMaxTolerance << ")"
                 << "\nDecoded frame: format="
                 << media::VideoPixelFormatToString(decoded.format())
                 << ", visible_rect=" << decoded.visible_rect().ToString()
                 << ", timestamp=" << decoded.timestamp()
                 << "\nDecoded PNG (base64): data:image/png;base64,"
                 << VideoFrameToBase64PNG(decoded)
                 << "\nExpected frame: format="
                 << media::VideoPixelFormatToString(expected_frame->format())
                 << ", visible_rect="
                 << expected_frame->visible_rect().ToString()
                 << ", timestamp=" << expected_frame->timestamp()
                 << "\nExpected PNG (base64): data:image/png;base64,"
                 << VideoFrameToBase64PNG(*expected_frame);
    }

    EXPECT_LE(diff_pixels, max_allowed_diff_pixels);
  }

  base::test::TaskEnvironment task_environment_;
  openscreen_platform::TaskRunner task_runner_;

  mojo::Receiver<mojom::ResourceProvider> resource_provider_receiver_;
  mojo::Receiver<mojom::SessionObserver> session_observer_receiver_;

  std::unique_ptr<NiceMock<FakeVideoCaptureHost>> video_host_;
  std::unique_ptr<NiceMock<MockSocketFactory>> socket_factory_;
  std::unique_ptr<OpenscreenSessionHost> session_host_;

  std::unique_ptr<TestReceiverEnvironment> receiver_env_;
  std::unique_ptr<TestReceiverMessagePort> receiver_message_port_;
  ReceiverSessionClientHelper receiver_client_;
  std::unique_ptr<openscreen::cast::ReceiverSession> receiver_session_;
  std::unique_ptr<ReceiverConsumer> consumer_;

  bool drop_outgoing_packets_ = false;
  base::OnceClosure did_start_cb_;
  base::OnceClosure video_host_started_cb_;
  base::OnceClosure session_host_deleted_cb_;
};

TEST_F(CastStreamingEndToEndTest, NV12StreamPixelFidelity) {
  SetUpStreamingSession(openscreen::cast::VideoCodec::kVp8);

  const gfx::Size frame_size(320, 240);
  const int num_frames = 5;

  for (int i = 0; i < num_frames; ++i) {
    base::RunLoop decode_loop;
    consumer_->set_on_frame_decoded(
        base::IgnoreArgs<scoped_refptr<media::VideoFrame>>(
            decode_loop.QuitClosure()));

    const int start_value = (i + 1) * 20;
    const base::TimeTicks capture_time =
        base::TimeTicks() + base::Milliseconds((i + 1) * 33);
    video_host_->SendOneFrame(frame_size, capture_time,
                              media::PIXEL_FORMAT_NV12, start_value);

    decode_loop.Run();
    task_environment_.FastForwardBy(base::Milliseconds(33));

    scoped_refptr<media::VideoFrame> decoded =
        consumer_->decoded_frames().back();
    ASSERT_TRUE(decoded);
    VerifyPixelFidelity(*decoded, media::PIXEL_FORMAT_NV12, frame_size,
                        start_value);
  }

  EXPECT_EQ(consumer_->decoded_frames().size(),
            static_cast<size_t>(num_frames));
}

TEST_F(CastStreamingEndToEndTest, I420StreamPixelFidelity) {
  SetUpStreamingSession(openscreen::cast::VideoCodec::kVp8);

  const gfx::Size frame_size(320, 240);
  const int num_frames = 3;

  for (int i = 0; i < num_frames; ++i) {
    base::RunLoop decode_loop;
    consumer_->set_on_frame_decoded(
        base::IgnoreArgs<scoped_refptr<media::VideoFrame>>(
            decode_loop.QuitClosure()));

    const int start_value = (i + 1) * 30;
    const base::TimeTicks capture_time =
        base::TimeTicks() + base::Milliseconds((i + 1) * 33);
    video_host_->SendOneFrame(frame_size, capture_time,
                              media::PIXEL_FORMAT_I420, start_value);

    decode_loop.Run();
    task_environment_.FastForwardBy(base::Milliseconds(33));

    scoped_refptr<media::VideoFrame> decoded =
        consumer_->decoded_frames().back();
    ASSERT_TRUE(decoded);
    VerifyPixelFidelity(*decoded, media::PIXEL_FORMAT_I420, frame_size,
                        start_value);
  }

  EXPECT_EQ(consumer_->decoded_frames().size(),
            static_cast<size_t>(num_frames));
}

TEST_F(CastStreamingEndToEndTest, VP9StreamPixelFidelity) {
  SetUpStreamingSession(openscreen::cast::VideoCodec::kVp9);

  const gfx::Size frame_size(320, 240);
  const int num_frames = 3;

  for (int i = 0; i < num_frames; ++i) {
    base::RunLoop decode_loop;
    consumer_->set_on_frame_decoded(
        base::IgnoreArgs<scoped_refptr<media::VideoFrame>>(
            decode_loop.QuitClosure()));

    const int start_value = (i + 1) * 25;
    const base::TimeTicks capture_time =
        base::TimeTicks() + base::Milliseconds((i + 1) * 33);
    video_host_->SendOneFrame(frame_size, capture_time,
                              media::PIXEL_FORMAT_NV12, start_value);

    decode_loop.Run();
    task_environment_.FastForwardBy(base::Milliseconds(33));

    scoped_refptr<media::VideoFrame> decoded =
        consumer_->decoded_frames().back();
    ASSERT_TRUE(decoded);
    VerifyPixelFidelity(*decoded, media::PIXEL_FORMAT_NV12, frame_size,
                        start_value);
  }

  EXPECT_EQ(consumer_->decoded_frames().size(),
            static_cast<size_t>(num_frames));
}

TEST_F(CastStreamingEndToEndTest, DynamicResolutionChange) {
  SetUpStreamingSession(openscreen::cast::VideoCodec::kVp8);

  const gfx::Size size1(320, 240);
  const gfx::Size size2(640, 360);
  const gfx::Size size3(480, 270);

  // 1. Send frame at initial resolution 320x240.
  {
    base::RunLoop decode_loop;
    consumer_->set_on_frame_decoded(
        base::IgnoreArgs<scoped_refptr<media::VideoFrame>>(
            decode_loop.QuitClosure()));

    const base::TimeTicks capture_time =
        base::TimeTicks() + base::Milliseconds(33);
    video_host_->SendOneFrame(size1, capture_time, media::PIXEL_FORMAT_NV12,
                              10);

    decode_loop.Run();
    task_environment_.FastForwardBy(base::Milliseconds(33));

    scoped_refptr<media::VideoFrame> decoded =
        consumer_->decoded_frames().back();
    ASSERT_TRUE(decoded);
    VerifyPixelFidelity(*decoded, media::PIXEL_FORMAT_NV12, size1, 10);
  }

  // 2. Change resolution to 640x360. Trigger resize and fast forward through
  // debounce.
  {
    base::RunLoop decode_loop;
    consumer_->set_on_frame_decoded(
        base::IgnoreArgs<scoped_refptr<media::VideoFrame>>(
            decode_loop.QuitClosure()));

    // First frame of new size starts the 250ms debounce timer.
    const base::TimeTicks trigger_time =
        base::TimeTicks() + base::Milliseconds(66);
    video_host_->SendOneFrame(size2, trigger_time, media::PIXEL_FORMAT_NV12,
                              20);

    decode_loop.Run();
    // Fast forward past the 250ms debounce timer to reconstruct encoder at
    // 640x360.
    task_environment_.FastForwardBy(base::Milliseconds(300));

    // Send next frame at 640x360 after encoder reconstruction.
    base::RunLoop resized_decode_loop;
    consumer_->set_on_frame_decoded(
        base::IgnoreArgs<scoped_refptr<media::VideoFrame>>(
            resized_decode_loop.QuitClosure()));

    const base::TimeTicks capture_time =
        base::TimeTicks() + base::Milliseconds(400);
    video_host_->SendOneFrame(size2, capture_time, media::PIXEL_FORMAT_NV12,
                              30);

    resized_decode_loop.Run();
    task_environment_.FastForwardBy(base::Milliseconds(33));

    scoped_refptr<media::VideoFrame> decoded =
        consumer_->decoded_frames().back();
    ASSERT_TRUE(decoded);
    VerifyPixelFidelity(*decoded, media::PIXEL_FORMAT_NV12, size2, 30);
  }

  // 3. Change resolution to 480x270.
  {
    base::RunLoop decode_loop;
    consumer_->set_on_frame_decoded(
        base::IgnoreArgs<scoped_refptr<media::VideoFrame>>(
            decode_loop.QuitClosure()));

    const base::TimeTicks trigger_time =
        base::TimeTicks() + base::Milliseconds(450);
    video_host_->SendOneFrame(size3, trigger_time, media::PIXEL_FORMAT_NV12,
                              40);

    decode_loop.Run();
    task_environment_.FastForwardBy(base::Milliseconds(300));

    base::RunLoop resized_decode_loop;
    consumer_->set_on_frame_decoded(
        base::IgnoreArgs<scoped_refptr<media::VideoFrame>>(
            resized_decode_loop.QuitClosure()));

    const base::TimeTicks capture_time =
        base::TimeTicks() + base::Milliseconds(800);
    video_host_->SendOneFrame(size3, capture_time, media::PIXEL_FORMAT_NV12,
                              50);

    resized_decode_loop.Run();
    task_environment_.FastForwardBy(base::Milliseconds(33));

    scoped_refptr<media::VideoFrame> decoded =
        consumer_->decoded_frames().back();
    ASSERT_TRUE(decoded);
    VerifyPixelFidelity(*decoded, media::PIXEL_FORMAT_NV12, size3, 50);
  }
}

TEST_F(CastStreamingEndToEndTest, PacketLossAndRetransmissionRecovery) {
  SetUpStreamingSession(openscreen::cast::VideoCodec::kVp8);

  const gfx::Size frame_size(320, 240);

  // 1. Send first frame normally.
  {
    base::RunLoop decode_loop;
    consumer_->set_on_frame_decoded(
        base::IgnoreArgs<scoped_refptr<media::VideoFrame>>(
            decode_loop.QuitClosure()));

    const base::TimeTicks capture_time =
        base::TimeTicks() + base::Milliseconds(33);
    video_host_->SendOneFrame(frame_size, capture_time,
                              media::PIXEL_FORMAT_NV12, 10);

    decode_loop.Run();
    task_environment_.FastForwardBy(base::Milliseconds(33));

    scoped_refptr<media::VideoFrame> decoded =
        consumer_->decoded_frames().back();
    ASSERT_TRUE(decoded);
    VerifyPixelFidelity(*decoded, media::PIXEL_FORMAT_NV12, frame_size, 10);
  }

  // 2. Drop outgoing packets for frame 2.
  drop_outgoing_packets_ = true;
  const base::TimeTicks drop_capture_time =
      base::TimeTicks() + base::Milliseconds(66);
  video_host_->SendOneFrame(frame_size, drop_capture_time,
                            media::PIXEL_FORMAT_NV12, 20);
  task_environment_.FastForwardBy(base::Milliseconds(33));

  // Verify no new decoded frame was produced yet because packets were dropped.
  EXPECT_EQ(consumer_->decoded_frames().size(), 1u);

  // 3. Resume packet transmission and send frame 3. The receiver NACKs frame 2,
  // prompting the sender to retransmit frame 2 packets, successfully decoding
  // both.
  drop_outgoing_packets_ = false;
  {
    base::RunLoop decode_loop;
    int frames_decoded = 0;
    consumer_->set_on_frame_decoded(
        base::BindLambdaForTesting([&frames_decoded, &decode_loop](
                                       scoped_refptr<media::VideoFrame> frame) {
          ++frames_decoded;
          if (frames_decoded >= 2) {
            decode_loop.Quit();
          }
        }));

    const base::TimeTicks recover_capture_time =
        base::TimeTicks() + base::Milliseconds(99);
    video_host_->SendOneFrame(frame_size, recover_capture_time,
                              media::PIXEL_FORMAT_NV12, 30);

    decode_loop.Run();
    task_environment_.FastForwardBy(base::Milliseconds(33));

    EXPECT_EQ(consumer_->decoded_frames().size(), 3u);
    VerifyPixelFidelity(*consumer_->decoded_frames()[1],
                        media::PIXEL_FORMAT_NV12, frame_size, 20);
    VerifyPixelFidelity(*consumer_->decoded_frames()[2],
                        media::PIXEL_FORMAT_NV12, frame_size, 30);
  }
}

TEST_F(CastStreamingEndToEndTest, StreamPauseAndResume) {
  SetUpStreamingSession(openscreen::cast::VideoCodec::kVp8);

  const gfx::Size frame_size(320, 240);

  // 1. Send initial frame.
  {
    base::RunLoop decode_loop;
    consumer_->set_on_frame_decoded(
        base::IgnoreArgs<scoped_refptr<media::VideoFrame>>(
            decode_loop.QuitClosure()));

    const base::TimeTicks capture_time =
        base::TimeTicks() + base::Milliseconds(33);
    video_host_->SendOneFrame(frame_size, capture_time,
                              media::PIXEL_FORMAT_NV12, 10);

    decode_loop.Run();
    task_environment_.FastForwardBy(base::Milliseconds(33));

    scoped_refptr<media::VideoFrame> decoded =
        consumer_->decoded_frames().back();
    ASSERT_TRUE(decoded);
    VerifyPixelFidelity(*decoded, media::PIXEL_FORMAT_NV12, frame_size, 10);
  }

  // 2. Pause the video capture stream.
  video_host_->Pause(base::UnguessableToken());
  EXPECT_TRUE(video_host_->paused());
  task_environment_.FastForwardBy(base::Seconds(2));

  // 3. Resume the video capture stream.
  video_host_->Resume(base::UnguessableToken(), base::UnguessableToken(),
                      video_host_->GetVideoCaptureParams());
  EXPECT_FALSE(video_host_->paused());

  // 4. Send frame after resume (with monotonic timestamp).
  {
    base::RunLoop decode_loop;
    consumer_->set_on_frame_decoded(
        base::IgnoreArgs<scoped_refptr<media::VideoFrame>>(
            decode_loop.QuitClosure()));

    const base::TimeTicks capture_time =
        base::TimeTicks() + base::Seconds(2) + base::Milliseconds(33);
    video_host_->SendOneFrame(frame_size, capture_time,
                              media::PIXEL_FORMAT_NV12, 20);

    decode_loop.Run();
    task_environment_.FastForwardBy(base::Milliseconds(33));

    scoped_refptr<media::VideoFrame> decoded =
        consumer_->decoded_frames().back();
    ASSERT_TRUE(decoded);
    VerifyPixelFidelity(*decoded, media::PIXEL_FORMAT_NV12, frame_size, 20);
  }

  EXPECT_EQ(consumer_->decoded_frames().size(), 2u);
}

// Verifies that enabling the performance overlay feature on an NV12 stream
// processes frames end-to-end without crashing during plane copying (which
// previously assumed I420 input), delivering valid frames to the receiver.
// Pixel fidelity checks are intentionally omitted because the overlay burns
// dynamic performance text/graphics into the video frames.
TEST_F(CastStreamingEndToEndTest, PerformanceOverlayWithNV12) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(media::kCastStreamingPerformanceOverlay);

  SetUpStreamingSession(openscreen::cast::VideoCodec::kVp8);

  const gfx::Size frame_size(320, 240);
  base::RunLoop decode_loop;
  consumer_->set_on_frame_decoded(
      base::IgnoreArgs<scoped_refptr<media::VideoFrame>>(
          decode_loop.QuitClosure()));

  const base::TimeTicks capture_time =
      base::TimeTicks() + base::Milliseconds(33);
  video_host_->SendOneFrame(frame_size, capture_time, media::PIXEL_FORMAT_NV12,
                            50);

  decode_loop.Run();

  scoped_refptr<media::VideoFrame> decoded = consumer_->decoded_frames().back();
  ASSERT_TRUE(decoded);
  EXPECT_EQ(decoded->format(), media::PIXEL_FORMAT_I420);
  EXPECT_EQ(decoded->visible_rect().size(), frame_size);
}

}  // namespace mirroring
