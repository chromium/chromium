// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "components/mirroring/mojom/resource_provider.mojom.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "components/mirroring/service/fake_socket_factory.h"
#include "components/mirroring/service/fake_video_capture_host.h"
#include "components/mirroring/service/mirroring_features.h"
#include "components/mirroring/service/openscreen_session_host.h"
#include "components/mirroring/service/simulated_network_link.h"
#include "components/mirroring/service/test_receiver_session_helpers.h"
#include "components/mirroring/service/text_document_content.h"
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
  CastStreamingEndToEndTest() : CastStreamingEndToEndTest(std::nullopt) {}

  ~CastStreamingEndToEndTest() override {
    CastStreamingEndToEndTest::StopSession();
    media::cast::FakeOpenscreenClock::ClearTickClock();
    media::cast::encoding_support::ClearHardwareCodecDenyListForTesting();
  }

  void TearDown() override { StopSession(); }

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
  struct ScopedSimulationFeatures {
    explicit ScopedSimulationFeatures(std::optional<int> max_bitrate_mbps) {
      if (max_bitrate_mbps.has_value()) {
        feature_list.InitWithFeaturesAndParameters(
            {{media::kCastStreamingMaxVideoBitrate,
              {{"max_bitrate_mbps", base::NumberToString(*max_bitrate_mbps)}}},
             {features::kCastStreamingGradualBandwidthBackoff, {}}},
            {});
      }
    }
    base::test::ScopedFeatureList feature_list;
  };

  explicit CastStreamingEndToEndTest(std::optional<int> max_bitrate_mbps)
      : scoped_features_(max_bitrate_mbps),
        task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
        resource_provider_receiver_(
            static_cast<mojom::ResourceProvider*>(this)),
        session_observer_receiver_(static_cast<mojom::SessionObserver*>(this)) {
    media::cast::FakeOpenscreenClock::SetTickClock(
        task_environment_.GetMockTickClock());
    media::cast::encoding_support::ClearHardwareCodecDenyListForTesting();
  }

  virtual void StopSession() {
    // Drop any packets still in flight on the simulated links.
    uplink_.reset();
    downlink_.reset();
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
    if (uplink_) {
      uplink_->Send(packet);
      return;
    }
    DeliverToReceiver(packet);
  }

  void DeliverToReceiver(const std::vector<uint8_t>& packet) {
    if (receiver_env_) {
      receiver_env_->DeliverPacket(
          openscreen::IPEndpoint{openscreen::IPAddress{127, 0, 0, 1}, 5678},
          packet);
    }
  }

  void OnReceiverRtcpPacket(const std::vector<uint8_t>& packet) {
    if (downlink_) {
      downlink_->Send(packet);
      return;
    }
    DeliverToSender(packet);
  }

  void DeliverToSender(const std::vector<uint8_t>& packet) {
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

  ScopedSimulationFeatures scoped_features_;
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

  // If set, packets in each direction are sent over these simulated links
  // instead of being delivered synchronously.
  std::unique_ptr<SimulatedNetworkLink> uplink_;
  std::unique_ptr<SimulatedNetworkLink> downlink_;
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

// Simulates mirroring a text document (e.g. Google Docs with 11pt text) over a
// healthy 5 GHz Wi-Fi link, and checks that the video bitrate stays high.
//
// This reproduces a customer report where, on such a link, the video bitrate
// collapsed from ~4.4 Mbps to ~0.45 Mbps within ~1.5 s of the content starting
// to change after being static, then recovered at ~+10% every 500 ms, taking
// ~12 s to reach the cap.
//
// Everything runs in one process on mock time: the real OpenscreenSessionHost
// (VideoSender, libvpx, Open Screen Sender) streams to a real Open Screen
// Receiver, with packets in both directions going through a
// SimulatedNetworkLink. Encoding and decoding take zero mock time.
class CastStreamingSimulationTest : public CastStreamingEndToEndTest {
 public:
  static constexpr int kDefaultMaxBitrateMbps = 5;
  static constexpr base::TimeDelta kSampleInterval = base::Milliseconds(500);

  CastStreamingSimulationTest()
      : CastStreamingEndToEndTest(kDefaultMaxBitrateMbps) {}

  struct Scenario {
    openscreen::cast::VideoCodec codec = openscreen::cast::VideoCodec::kVp8;
    // Kept below 640x480 so the software encoder runs single-threaded. The
    // multithreaded libvpx VP8 encoder spins while waiting on its worker
    // threads, and with several test processes running in parallel on a small
    // bot (as in the CQ flakiness check) it can take minutes per test. The
    // bandwidth logic under test does not depend on the frame size.
    gfx::Size frame_size{640, 360};
    base::TimeDelta initial_static_duration = base::Seconds(6);
    base::TimeDelta active_duration = base::Seconds(4);
    base::TimeDelta final_static_duration = base::Seconds(4);
    // Frame rate and scroll speed while the content is changing.
    int active_fps = 20;
    int scroll_pixels_per_frame = 8;
    // The maximum video bitrate, matching the customer's configuration.
    int max_bitrate_mbps = kDefaultMaxBitrateMbps;
    // Optional transient bottleneck capacity reduction during the active phase
    // (relative to the start of the active phase).
    std::optional<base::TimeDelta> transient_choke_offset;
    base::TimeDelta transient_choke_duration;
    int64_t transient_choke_capacity_bps = 0;
    SimulatedNetworkLink::Config uplink;
    SimulatedNetworkLink::Config downlink;
  };

  struct Sample {
    base::TimeDelta time;
    const char* phase = "";
    int estimate_bps = 0;
    uint32_t usable_bps = 0;
    uint32_t encoder_bps = 0;
    int frames_dropped = 0;
    int frames_dropped_by_encoder = 0;
    int frames_captured = 0;
    int frames_decoded = 0;
    int64_t uplink_kbps = 0;
    int64_t uplink_lost = 0;
  };

  // Configuration resembling a healthy 5 GHz Wi-Fi link where 802.11 MAC-layer
  // retries absorb RF bit errors as latency jitter (8-52 ms RTT) without
  // IP-level UDP packet drops, and capacity is well above 10 Mbps.
  static Scenario HealthyWifiScenario() {
    Scenario scenario;
    scenario.uplink.capacity_bps = 30'000'000;
    scenario.uplink.propagation_delay = base::Milliseconds(4);
    scenario.uplink.max_jitter = base::Milliseconds(22);
    scenario.uplink.queue_limit_bytes = 256 * 1024;
    scenario.uplink.random_loss_rate = 0.0;
    scenario.uplink.loss_bursts_per_second = 0.0;
    scenario.uplink.max_loss_burst_packets = 5;
    scenario.uplink.seed = 1;
    scenario.downlink = scenario.uplink;
    scenario.downlink.seed = 2;
    return scenario;
  }

  // Configuration with heavy IP-level UDP packet loss bursts (~2 bursts/s of
  // 1-5 packets plus 0.5% random loss).
  static Scenario LossyWifiScenario() {
    Scenario scenario = HealthyWifiScenario();
    scenario.uplink.random_loss_rate = 0.005;
    scenario.uplink.loss_bursts_per_second = 2.0;
    scenario.downlink.random_loss_rate = 0.005;
    return scenario;
  }

  // Configuration resembling a lossless Wi-Fi link with a 100-120 ms RTT, as
  // seen in some classrooms. This is well within the 200 ms playout delay, but
  // above the Sender's minimum in-flight window, so each frame waits for its
  // ACK longer than the minimum window allows.
  static Scenario HighLatencyWifiScenario() {
    Scenario scenario = HealthyWifiScenario();
    scenario.uplink.propagation_delay = base::Milliseconds(50);
    scenario.uplink.max_jitter = base::Milliseconds(10);
    scenario.downlink.propagation_delay = base::Milliseconds(50);
    scenario.downlink.max_jitter = base::Milliseconds(10);
    return scenario;
  }

  void StopSession() override {
    scenario_running_ = false;
    refresh_capture_timer_.Stop();
    active_timer_.Stop();
    sample_timer_.Stop();
    cached_source_frame_.reset();
    CastStreamingEndToEndTest::StopSession();
  }

 protected:
  // Runs `scenario` and populates `samples` every `kSampleInterval`, starting
  // when the content starts being captured.
  void RunScenario(const Scenario& scenario, std::vector<Sample>& samples) {
    samples.clear();
    ASSERT_EQ(scenario.max_bitrate_mbps, kDefaultMaxBitrateMbps);

    content_ =
        std::make_unique<TextDocumentContent>(scenario.frame_size, /*seed=*/7);
    uplink_ = std::make_unique<SimulatedNetworkLink>(
        scenario.uplink, task_environment_.GetMockTickClock(),
        task_runner_ref(),
        base::BindRepeating(&CastStreamingSimulationTest::DeliverToReceiver,
                            base::Unretained(this)));
    downlink_ = std::make_unique<SimulatedNetworkLink>(
        scenario.downlink, task_environment_.GetMockTickClock(),
        task_runner_ref(),
        base::BindRepeating(&CastStreamingSimulationTest::DeliverToSender,
                            base::Unretained(this)));

    ASSERT_NO_FATAL_FAILURE(SetUpStreamingSession(scenario.codec));
    consumer_->set_retain_decoded_frames(false);

    const base::TimeTicks start_time = base::TimeTicks::Now();
    start_time_ = start_time;
    const base::TimeTicks active_start =
        start_time_ + scenario.initial_static_duration;
    const base::TimeTicks active_end = active_start + scenario.active_duration;
    const base::TimeDelta total_duration = scenario.initial_static_duration +
                                           scenario.active_duration +
                                           scenario.final_static_duration;

    // Like the real capturer, deliver a (possibly unchanged) frame whenever a
    // refresh frame is requested. After the active scrolling phase completes,
    // throttle static refresh captures to ~1 Hz so long post-activity tails
    // (such as 60+ second histogram sessions) do not encode hundreds of
    // identical frames.
    ON_CALL(*video_host_, RequestRefreshFrame(::testing::_))
        .WillByDefault([this, active_end](const base::UnguessableToken&) {
          const base::TimeDelta delay = base::TimeTicks::Now() >= active_end
                                            ? base::Milliseconds(950)
                                            : base::TimeDelta();
          refresh_capture_timer_.Start(
              FROM_HERE, delay, this,
              &CastStreamingSimulationTest::CaptureFrame);
        });
    ON_CALL(*video_host_,
            ReleaseBuffer(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault([this](const base::UnguessableToken&, int32_t buffer_id,
                              const media::VideoCaptureFeedback& feedback) {
          if (feedback.resource_utilization >= 0.0) {
            ++feedback_with_utilization_count_;
            last_resource_utilization_ = feedback.resource_utilization;
          }
          video_host_->ReturnBuffer(buffer_id);
        });

    base::HistogramTester histograms;
    scenario_running_ = true;

    // Initial frame; after that the session's refresh timer drives capture
    // while the content is static.
    CaptureFrame();

    const SimulatedNetworkLink::Stats* uplink_stats = &uplink_->stats();
    int64_t last_uplink_bytes = 0;
    sample_timer_.Start(
        FROM_HERE, kSampleInterval, base::BindLambdaForTesting([&] {
          const base::TimeTicks now = base::TimeTicks::Now();
          Sample sample;
          sample.time = now - start_time_;
          sample.phase = now <= active_start
                             ? "static"
                             : (now <= active_end ? "ACTIVE" : "static");
          sample.estimate_bps =
              session_host_->GetEstimatedNetworkBandwidthForTesting();
          sample.usable_bps =
              session_host_->GetVideoNetworkBandwidthForTesting();
          sample.encoder_bps =
              session_host_->GetVideoEncoderBitrateForTesting();
          const std::optional<int> frames_dropped =
              session_host_->GetVideoFramesDroppedForTesting();
          const std::optional<int> frames_dropped_by_encoder =
              session_host_->GetVideoFramesDroppedByEncoderForTesting();
          EXPECT_TRUE(frames_dropped.has_value());
          EXPECT_TRUE(frames_dropped_by_encoder.has_value());
          sample.frames_dropped = frames_dropped.value_or(0);
          sample.frames_dropped_by_encoder =
              frames_dropped_by_encoder.value_or(0);
          sample.frames_captured = frames_captured_;
          sample.frames_decoded = consumer_->num_frames_decoded();
          sample.uplink_kbps =
              (uplink_stats->bytes_sent - last_uplink_bytes) * 8 / 500;
          last_uplink_bytes = uplink_stats->bytes_sent;
          sample.uplink_lost = uplink_stats->packets_lost_randomly +
                               uplink_stats->packets_dropped_by_queue;
          samples.push_back(sample);
        }));

    task_environment_.FastForwardBy(scenario.initial_static_duration);

    // Content changes: scroll the document at `active_fps`.
    refresh_capture_timer_.Stop();
    active_timer_.Start(FROM_HERE, base::Seconds(1) / scenario.active_fps,
                        base::BindLambdaForTesting([&] {
                          scroll_offset_ += scenario.scroll_pixels_per_frame;
                          CaptureFrame();
                        }));
    if (scenario.transient_choke_offset &&
        scenario.transient_choke_duration.is_positive() &&
        scenario.transient_choke_capacity_bps > 0) {
      ASSERT_LE(
          *scenario.transient_choke_offset + scenario.transient_choke_duration,
          scenario.active_duration);
      task_environment_.FastForwardBy(*scenario.transient_choke_offset);
      uplink_->SetCapacityBps(scenario.transient_choke_capacity_bps);
      task_environment_.FastForwardBy(scenario.transient_choke_duration);
      uplink_->SetCapacityBps(scenario.uplink.capacity_bps);
      task_environment_.FastForwardBy(scenario.active_duration -
                                      *scenario.transient_choke_offset -
                                      scenario.transient_choke_duration);
    } else {
      task_environment_.FastForwardBy(scenario.active_duration);
    }
    active_timer_.Stop();

    task_environment_.FastForwardBy(scenario.final_static_duration);
    refresh_capture_timer_.Stop();
    sample_timer_.Stop();
    scenario_running_ = false;
    ASSERT_FALSE(samples.empty());
    ASSERT_EQ(samples.back().time, total_duration);

    LogSamples(samples, *uplink_, *downlink_);

    // Breakdown of frame drops, by media::cast::CastStreamingFrameDropReason.
    std::string drop_reasons;
    for (const base::Bucket& bucket :
         histograms.GetAllSamples("CastStreaming.Sender.Video.FrameDropped")) {
      drop_reasons += base::StringPrintf(" reason%d=%d", bucket.min,
                                         static_cast<int>(bucket.count));
    }
    LOG(INFO) << "Frame drops by reason:" << drop_reasons;
  }

  // Returns a rendered frame for `scroll_offset`, reusing the cached frame when
  // `scroll_offset` has not changed.
  scoped_refptr<media::VideoFrame> GetRenderedFrame(int scroll_offset) {
    if (!cached_source_frame_ || cached_scroll_offset_ != scroll_offset) {
      cached_scroll_offset_ = scroll_offset;
      cached_source_frame_ = content_->Render(scroll_offset, base::TimeDelta());
    }
    return cached_source_frame_;
  }

  // Captures the current content and delivers it to the session.
  void CaptureFrame() {
    if (!scenario_running_ || !video_host_) {
      return;
    }
    const base::TimeTicks now = base::TimeTicks::Now();
    // Frame timestamps must be strictly increasing.
    if (now <= last_capture_time_) {
      return;
    }
    last_capture_time_ = now;
    scoped_refptr<media::VideoFrame> frame = GetRenderedFrame(scroll_offset_);
    if (video_host_->SendFrame(*frame, now) >= 0) {
      ++frames_captured_;
    }
  }

  void LogSamples(const std::vector<Sample>& samples,
                  const SimulatedNetworkLink& uplink,
                  const SimulatedNetworkLink& downlink) {
    std::string table = base::StringPrintf(
        "\n%6s %-6s %8s %8s %8s %6s %5s %6s %6s %7s %5s\n", "t(s)", "phase",
        "est_kbps", "use_kbps", "enc_kbps", "drops", "edrop", "capt", "decod",
        "tx_kbps", "lost");
    for (const Sample& s : samples) {
      table += base::StringPrintf(
          "%6.1f %-6s %8d %8u %8u %6d %5d %6d %6d %7" PRId64 " %5" PRId64 "\n",
          s.time.InSecondsF(), s.phase, s.estimate_bps / 1000,
          s.usable_bps / 1000, s.encoder_bps / 1000, s.frames_dropped,
          s.frames_dropped_by_encoder, s.frames_captured, s.frames_decoded,
          s.uplink_kbps, s.uplink_lost);
    }
    const SimulatedNetworkLink::Stats& up = uplink.stats();
    const SimulatedNetworkLink::Stats& down = downlink.stats();
    table += base::StringPrintf(
        "uplink: sent=%" PRId64 " delivered=%" PRId64 " lost=%" PRId64
        " queue_drops=%" PRId64 " max_queue_delay=%" PRId64
        "ms | downlink: sent=%" PRId64 " lost=%" PRId64,
        up.packets_sent, up.packets_delivered, up.packets_lost_randomly,
        up.packets_dropped_by_queue, up.max_queueing_delay.InMilliseconds(),
        down.packets_sent, down.packets_lost_randomly);
    LOG(INFO) << "Cast streaming simulation metrics:" << table;
  }

  // Checks the expected behavior on a healthy link: once the content starts
  // changing, the video bitrate never collapses, and any reduction is quickly
  // recovered.
  void ExpectHealthyBitrate(const std::vector<Sample>& samples,
                            const Scenario& scenario) {
    ASSERT_FALSE(samples.empty());
    const double cap = scenario.max_bitrate_mbps * 1e6;
    const base::TimeDelta active_start = scenario.initial_static_duration;

    uint32_t min_encoder_bps = std::numeric_limits<uint32_t>::max();
    base::TimeDelta min_time;
    base::TimeDelta longest_below_80;
    std::optional<base::TimeDelta> below_80_since;
    for (const Sample& s : samples) {
      if (s.time <= active_start) {
        continue;
      }
      if (s.encoder_bps < min_encoder_bps) {
        min_encoder_bps = s.encoder_bps;
        min_time = s.time;
      }
      if (s.encoder_bps < 0.8 * cap) {
        if (!below_80_since) {
          below_80_since = s.time;
        }
        longest_below_80 = std::max(longest_below_80, s.time - *below_80_since);
      } else {
        below_80_since.reset();
      }
    }

    EXPECT_GE(min_encoder_bps, 0.5 * cap)
        << "Video bitrate collapsed to " << min_encoder_bps / 1000
        << " kbps at t=" << min_time.InSecondsF()
        << "s on a healthy link (cap=" << cap / 1000 << " kbps).";
    EXPECT_LE(longest_below_80, base::Seconds(3))
        << "Video bitrate stayed below 80% of the cap for "
        << longest_below_80.InSecondsF() << "s.";
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_ref() {
    return base::SingleThreadTaskRunner::GetCurrentDefault();
  }

  std::unique_ptr<TextDocumentContent> content_;
  bool scenario_running_ = false;
  base::TimeTicks start_time_;
  base::TimeTicks last_capture_time_;
  int scroll_offset_ = 0;
  int frames_captured_ = 0;
  int feedback_with_utilization_count_ = 0;
  double last_resource_utilization_ = -1.0;
  int cached_scroll_offset_ = -1;
  scoped_refptr<media::VideoFrame> cached_source_frame_;
  base::OneShotTimer refresh_capture_timer_;
  base::RepeatingTimer active_timer_;
  base::RepeatingTimer sample_timer_;
};

TEST_F(CastStreamingSimulationTest, TextDocumentOnHealthyWifiVp8) {
  const Scenario scenario = HealthyWifiScenario();
  std::vector<Sample> samples;
  ASSERT_NO_FATAL_FAILURE(RunScenario(scenario, samples));
  ExpectHealthyBitrate(samples, scenario);
}

TEST_F(CastStreamingSimulationTest, TextDocumentOnHealthyWifiVp9) {
  Scenario scenario = HealthyWifiScenario();
  scenario.codec = openscreen::cast::VideoCodec::kVp9;
  std::vector<Sample> samples;
  ASSERT_NO_FATAL_FAILURE(RunScenario(scenario, samples));
  ExpectHealthyBitrate(samples, scenario);
}

// Verifies that when a transient bottleneck choke causes in-flight congestion
// drops during active scrolling, gradual bandwidth backoff reduces target
// bitrate multiplicatively (instead of collapsing to the floor) and fast
// recovery restores the full 5 Mbps bitrate once the bottleneck clears.
TEST_F(CastStreamingSimulationTest,
       GradualBackoffAndRecoveryOnTransientCongestion) {
  Scenario scenario = HealthyWifiScenario();
  scenario.transient_choke_offset = base::Seconds(2);
  scenario.transient_choke_duration = base::Milliseconds(250);
  scenario.transient_choke_capacity_bps = 400'000;
  std::vector<Sample> samples;
  ASSERT_NO_FATAL_FAILURE(RunScenario(scenario, samples));
  ExpectHealthyBitrate(samples, scenario);

  // Confirm congestion caused frame drops during the choke, triggering a
  // single 15% multiplicative backoff in usable bandwidth followed by immediate
  // fast recovery on the next sample interval.
  EXPECT_GT(samples.back().frames_dropped, 0);
  const base::TimeDelta choke_start =
      scenario.initial_static_duration + *scenario.transient_choke_offset;
  const size_t before_choke_idx =
      static_cast<size_t>(choke_start / kSampleInterval) - 1;
  ASSERT_LT(before_choke_idx + 2, samples.size());
  ASSERT_EQ(samples[before_choke_idx].time, choke_start);
  const uint32_t usable_before_choke = samples[before_choke_idx].usable_bps;
  const uint32_t usable_during_choke = samples[before_choke_idx + 1].usable_bps;
  const uint32_t usable_after_choke = samples[before_choke_idx + 2].usable_bps;
  // Fast recovery restores usable bandwidth to `usable_before_choke`, bounded
  // by the `0.8 * estimate_bps` good-network-citizen ceiling (which dips
  // slightly while packets lost during the choke remain in the estimator
  // window).
  const uint32_t expected_recovered_bps = std::min<uint32_t>(
      usable_before_choke, samples[before_choke_idx + 2].estimate_bps * 0.8);
  EXPECT_NEAR(usable_during_choke, usable_before_choke * 0.85, 10'000);
  EXPECT_GT(usable_after_choke, usable_during_choke);
  EXPECT_NEAR(usable_after_choke, expected_recovered_bps,
              expected_recovered_bps * 0.05);
  EXPECT_EQ(samples.back().encoder_bps, 5'000'000u);
}

// Verifies that a 60+ second streaming session populates real encoder-QP
// utilization feedback on released capture buffers and emits all four
// end-of-session quality histograms upon teardown.
TEST_F(CastStreamingSimulationTest,
       RecordsSessionQualityHistogramsAndCaptureFeedback) {
  base::HistogramTester histograms;
  Scenario scenario = HealthyWifiScenario();
  scenario.initial_static_duration = base::Seconds(5);
  scenario.active_duration = base::Seconds(5);
  scenario.final_static_duration = base::Seconds(55);
  std::vector<Sample> samples;
  ASSERT_NO_FATAL_FAILURE(RunScenario(scenario, samples));
  ExpectHealthyBitrate(samples, scenario);

  EXPECT_GT(feedback_with_utilization_count_, 0);
  EXPECT_GE(last_resource_utilization_, 0.0);
  EXPECT_LE(last_resource_utilization_, 1.0);

  // Stop the session to trigger RecordSessionQualityMetrics().
  StopSession();

  histograms.ExpectTotalCount("CastStreaming.Sender.Video.AverageLossiness", 1);
  histograms.ExpectTotalCount("CastStreaming.Sender.Video.AverageTargetBitrate",
                              1);
  histograms.ExpectTotalCount(
      "CastStreaming.Sender.Video.PercentTimeAtMaxBitrate", 1);
  histograms.ExpectTotalCount(
      "CastStreaming.Sender.Video.BandwidthReductionsPerMinute", 1);
}

// Verifies that video bitrate remains healthy under heavy IP-level UDP packet
// loss bursts (~2 bursts/s of 1-5 packets plus 0.5% random loss).
// TODO(crbug.com/572902679): Enable once Open Screen fixes ReceiverImpl's NACK
// timer and queue check (crrev.com/c/8681981) and rolls into Chromium.
TEST_F(CastStreamingSimulationTest, DISABLED_TextDocumentOnLossyWifiVp8) {
  const Scenario scenario = LossyWifiScenario();
  std::vector<Sample> samples;
  ASSERT_NO_FATAL_FAILURE(RunScenario(scenario, samples));
  ExpectHealthyBitrate(samples, scenario);
}

// Verifies that video bitrate remains healthy on a lossless link whose RTT is
// above the Sender's minimum in-flight window but within the playout delay.
TEST_F(CastStreamingSimulationTest, TextDocumentOnLosslessHighLatencyWifiVp8) {
  const Scenario scenario = HighLatencyWifiScenario();
  std::vector<Sample> samples;
  ASSERT_NO_FATAL_FAILURE(RunScenario(scenario, samples));
  ExpectHealthyBitrate(samples, scenario);
  EXPECT_LE(samples.back().frames_dropped, samples.back().frames_captured / 50);
}

}  // namespace mirroring
