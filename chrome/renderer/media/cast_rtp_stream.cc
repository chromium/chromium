// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_rtp_stream.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/renderer/media/cast_session.h"
#include "chrome/renderer/media/cast_udp_transport.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/video_encode_accelerator.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_parameters.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_sender.h"
#include "media/cast/net/cast_transport_config.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_sink.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_utils.h"
#include "ui/gfx/geometry/size.h"

using media::cast::FrameSenderConfig;

namespace {

// The maximum number of milliseconds that should elapse since the last video
// frame was received from the video source, before requesting refresh frames.
const int kRefreshIntervalMilliseconds = 250;

// The maximum number of refresh video frames to request/receive.  After this
// limit (60 * 250ms = 15 seconds), refresh frame requests will stop being made.
const int kMaxConsecutiveRefreshFrames = 60;

FrameSenderConfig DefaultOpusConfig() {
  FrameSenderConfig config;
  config.rtp_payload_type = media::cast::RtpPayloadType::AUDIO_OPUS;
  config.sender_ssrc = 1;
  config.receiver_ssrc = 2;
  config.rtp_timebase = media::cast::kDefaultAudioSamplingRate;
  config.channels = 2;
  config.min_bitrate = config.max_bitrate = config.start_bitrate =
      media::cast::kDefaultAudioEncoderBitrate;
  config.max_frame_rate = 100;  // 10 ms audio frames
  config.codec = media::cast::CODEC_AUDIO_OPUS;
  return config;
}

FrameSenderConfig DefaultVp8Config() {
  FrameSenderConfig config;
  config.rtp_payload_type = media::cast::RtpPayloadType::VIDEO_VP8;
  config.sender_ssrc = 11;
  config.receiver_ssrc = 12;
  config.rtp_timebase = media::cast::kVideoFrequency;
  config.channels = 1;
  config.max_bitrate = media::cast::kDefaultMaxVideoBitrate;
  config.min_bitrate = media::cast::kDefaultMinVideoBitrate;
  config.max_frame_rate = media::cast::kDefaultMaxFrameRate;
  config.codec = media::cast::CODEC_VIDEO_VP8;
  return config;
}

FrameSenderConfig DefaultH264Config() {
  FrameSenderConfig config;
  config.rtp_payload_type = media::cast::RtpPayloadType::VIDEO_H264;
  config.sender_ssrc = 11;
  config.receiver_ssrc = 12;
  config.rtp_timebase = media::cast::kVideoFrequency;
  config.channels = 1;
  config.max_bitrate = media::cast::kDefaultMaxVideoBitrate;
  config.min_bitrate = media::cast::kDefaultMinVideoBitrate;
  config.max_frame_rate = media::cast::kDefaultMaxFrameRate;
  config.codec = media::cast::CODEC_VIDEO_H264;
  return config;
}

FrameSenderConfig DefaultRemotingAudioConfig() {
  FrameSenderConfig config;
  config.rtp_payload_type = media::cast::RtpPayloadType::REMOTE_AUDIO;
  config.sender_ssrc = 3;
  config.receiver_ssrc = 4;
  config.codec = media::cast::CODEC_AUDIO_REMOTE;
  config.rtp_timebase = media::cast::kRemotingRtpTimebase;
  config.max_bitrate = 1000000;
  config.min_bitrate = 0;
  config.channels = 2;
  config.max_frame_rate = 100;  // 10 ms audio frames

  return config;
}

FrameSenderConfig DefaultRemotingVideoConfig() {
  FrameSenderConfig config;
  config.rtp_payload_type = media::cast::RtpPayloadType::REMOTE_VIDEO;
  config.sender_ssrc = 13;
  config.receiver_ssrc = 14;
  config.codec = media::cast::CODEC_VIDEO_REMOTE;
  config.rtp_timebase = media::cast::kRemotingRtpTimebase;
  config.max_bitrate = 10000000;
  config.min_bitrate = 0;
  config.channels = 1;
  config.max_frame_rate = media::cast::kDefaultMaxFrameRate;
  return config;
}

std::vector<FrameSenderConfig> SupportedAudioConfigs(bool for_remoting_stream) {
  if (for_remoting_stream)
    return {DefaultRemotingAudioConfig()};
  else
    return {DefaultOpusConfig()};
}

std::vector<FrameSenderConfig> SupportedVideoConfigs(bool for_remoting_stream) {
  if (for_remoting_stream)
    return {DefaultRemotingVideoConfig()};

  std::vector<FrameSenderConfig> supported_configs;
  // Prefer VP8 over H.264 for hardware encoder.
  if (CastRtpStream::IsHardwareVP8EncodingSupported())
    supported_configs.push_back(DefaultVp8Config());
  if (CastRtpStream::IsHardwareH264EncodingSupported())
    supported_configs.push_back(DefaultH264Config());

  // Propose the default software VP8 encoder, if no hardware encoders are
  // available.
  if (supported_configs.empty())
    supported_configs.push_back(DefaultVp8Config());

  return supported_configs;
}

}  // namespace

// This class receives MediaStreamTrack events and video frames from a
// MediaStreamVideoTrack.  It also includes a timer to request refresh frames
// when the capturer halts (e.g., a screen capturer stops delivering frames
// because the screen is not being updated).  When a halt is detected, refresh
// frames will be requested at regular intervals for a short period of time.
// This provides the video encoder, downstream, several copies of the last frame
// so that it may clear up lossy encoding artifacts.
//
// Threading: Video frames are received on the IO thread and then
// forwarded to media::cast::VideoFrameInput.  The inner class, Deliverer,
// handles this.  Otherwise, all methods and member variables of the outer class
// must only be accessed on the render thread.
class CastVideoSink : public base::SupportsWeakPtr<CastVideoSink>,
                      public blink::WebMediaStreamSink {
 public:
  // |track| provides data for this sink.
  // |error_callback| is called if video formats don't match.
  CastVideoSink(const blink::WebMediaStreamTrack& track,
                const CastRtpStream::ErrorCallback& error_callback)
      : track_(track),
        deliverer_(new Deliverer(error_callback)),
        consecutive_refresh_count_(0),
        expecting_a_refresh_frame_(false),
        is_connected_to_track_(false) {}

  ~CastVideoSink() override {
    if (is_connected_to_track_)
      blink::RemoveSinkFromMediaStreamTrack(track_, this);
  }

  // Attach this sink to a video track represented by |track_|.
  // Data received from the track will be submitted to |frame_input|.
  void AddToTrack(
      bool is_sink_secure,
      const scoped_refptr<media::cast::VideoFrameInput>& frame_input) {
    DCHECK(deliverer_);
    deliverer_->WillConnectToTrack(AsWeakPtr(), frame_input);
    refresh_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kRefreshIntervalMilliseconds),
        base::Bind(&CastVideoSink::OnRefreshTimerFired,
                   base::Unretained(this)));
    blink::AddSinkToMediaStreamTrack(
        track_, this, base::BindRepeating(&Deliverer::OnVideoFrame, deliverer_),
        is_sink_secure);
    is_connected_to_track_ = true;
  }

 private:
  class Deliverer : public base::RefCountedThreadSafe<Deliverer> {
   public:
    explicit Deliverer(const CastRtpStream::ErrorCallback& error_callback)
        : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
          error_callback_(error_callback) {}

    void WillConnectToTrack(
        base::WeakPtr<CastVideoSink> sink,
        scoped_refptr<media::cast::VideoFrameInput> frame_input) {
      DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
      sink_ = sink;
      frame_input_ = std::move(frame_input);
    }

    void OnVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                      base::TimeTicks estimated_capture_time) {
      main_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&CastVideoSink::DidReceiveFrame, sink_));

      const base::TimeTicks timestamp = estimated_capture_time.is_null()
                                            ? base::TimeTicks::Now()
                                            : estimated_capture_time;

      if (!(video_frame->format() == media::PIXEL_FORMAT_I420 ||
            video_frame->format() == media::PIXEL_FORMAT_YV12 ||
            video_frame->format() == media::PIXEL_FORMAT_I420A)) {
        error_callback_.Run("Incompatible video frame format.");
        return;
      }
      scoped_refptr<media::VideoFrame> frame = video_frame;
      // Drop alpha channel since we do not support it yet.
      if (frame->format() == media::PIXEL_FORMAT_I420A)
        frame = media::WrapAsI420VideoFrame(std::move(video_frame));

      // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
      TRACE_EVENT_INSTANT2("cast_perf_test", "ConsumeVideoFrame",
                           TRACE_EVENT_SCOPE_THREAD, "timestamp",
                           (timestamp - base::TimeTicks()).InMicroseconds(),
                           "time_delta", frame->timestamp().InMicroseconds());
      frame_input_->InsertRawVideoFrame(std::move(frame), timestamp);
    }

   private:
    friend class base::RefCountedThreadSafe<Deliverer>;
    ~Deliverer() {}

    const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
    const CastRtpStream::ErrorCallback error_callback_;

    // These are set on the main thread after construction, and before the first
    // call to OnVideoFrame() on the IO thread.  |sink_| may be passed around on
    // any thread, but must only be dereferenced on the main renderer thread.
    base::WeakPtr<CastVideoSink> sink_;
    scoped_refptr<media::cast::VideoFrameInput> frame_input_;

    DISALLOW_COPY_AND_ASSIGN(Deliverer);
  };

 private:
  void OnRefreshTimerFired() {
    ++consecutive_refresh_count_;
    if (consecutive_refresh_count_ >= kMaxConsecutiveRefreshFrames)
      refresh_timer_.Stop();  // Stop timer until receiving a non-refresh frame.

    DVLOG(1) << "CastVideoSink is requesting another refresh frame "
                "(consecutive count=" << consecutive_refresh_count_ << ").";
    expecting_a_refresh_frame_ = true;
    blink::RequestRefreshFrameFromVideoTrack(track_);
  }

  void DidReceiveFrame() {
    if (expecting_a_refresh_frame_) {
      // There is uncertainty as to whether the video frame was in response to a
      // refresh request.  However, if it was not, more video frames will soon
      // follow, and before the refresh timer can fire again.  Thus, the
      // behavior resulting from this logic will be correct.
      expecting_a_refresh_frame_ = false;
    } else {
      consecutive_refresh_count_ = 0;
      // The following re-starts the timer, scheduling it to fire at
      // kRefreshIntervalMilliseconds from now.
      refresh_timer_.Reset();
    }
  }

  const blink::WebMediaStreamTrack track_;
  const scoped_refptr<Deliverer> deliverer_;

  // Requests refresh frames at a constant rate while the source is paused, up
  // to a consecutive maximum.
  base::RepeatingTimer refresh_timer_;

  // Counter for the number of consecutive "refresh frames" requested.
  int consecutive_refresh_count_;

  // Set to true when a request for a refresh frame has been made.  This is
  // cleared once the next frame is received.
  bool expecting_a_refresh_frame_;

  bool is_connected_to_track_;

  DISALLOW_COPY_AND_ASSIGN(CastVideoSink);
};

// Receives audio data from a MediaStreamTrack. Data is submitted to
// media::cast::FrameInput.
//
// Threading: Audio frames are received on the real-time audio thread.
// Note that RemoveFromAudioTrack() is synchronous and we have
// gurantee that there will be no more audio data after calling it.
class CastAudioSink : public base::SupportsWeakPtr<CastAudioSink>,
                      public blink::WebMediaStreamAudioSink,
                      public media::AudioConverter::InputCallback {
 public:
  // |track| provides data for this sink.
  CastAudioSink(const blink::WebMediaStreamTrack& track,
                int output_channels,
                int output_sample_rate)
      : track_(track),
        output_channels_(output_channels),
        output_sample_rate_(output_sample_rate),
        current_input_bus_(nullptr),
        sample_frames_in_(0),
        sample_frames_out_(0) {}

  ~CastAudioSink() override {
    if (frame_input_.get())
      RemoveFromAudioTrack(this, track_);
  }

  // Add this sink to the track. Data received from the track will be
  // submitted to |frame_input|.
  void AddToTrack(
      const scoped_refptr<media::cast::AudioFrameInput>& frame_input) {
    DCHECK(frame_input.get());
    DCHECK(!frame_input_.get());
    // This member is written here and then accessed on the IO thread
    // We will not get data until AddToAudioTrack is called so it is
    // safe to access this member now.
    frame_input_ = frame_input;
    AddToAudioTrack(this, track_);
  }

 protected:
  // Called on real-time audio thread.
  void OnData(const media::AudioBus& input_bus,
              base::TimeTicks estimated_capture_time) override {
    DCHECK(input_params_.IsValid());
    DCHECK_EQ(input_bus.channels(), input_params_.channels());
    DCHECK_EQ(input_bus.frames(), input_params_.frames_per_buffer());
    DCHECK(!estimated_capture_time.is_null());
    DCHECK(converter_.get());

    // Determine the duration of the audio signal enqueued within |converter_|.
    const base::TimeDelta signal_duration_already_buffered =
        (sample_frames_in_ * base::TimeDelta::FromSeconds(1) /
             input_params_.sample_rate()) -
        (sample_frames_out_ * base::TimeDelta::FromSeconds(1) /
             output_sample_rate_);
    DVLOG(2) << "Audio reference time adjustment: -("
             << signal_duration_already_buffered.InMicroseconds() << " us)";
    const base::TimeTicks capture_time_of_first_converted_sample =
        estimated_capture_time - signal_duration_already_buffered;

    // Convert the entire input signal.  AudioConverter is efficient in that no
    // additional copying or conversion will occur if the input signal is in the
    // same format as the output.  Note that, while the number of sample frames
    // provided as input is always the same, the chunk size (and the size of the
    // |audio_bus| here) can be variable.  This is not an issue since
    // media::cast::AudioFrameInput can handle variable-sized AudioBuses.
    std::unique_ptr<media::AudioBus> audio_bus =
        media::AudioBus::Create(output_channels_, converter_->ChunkSize());
    // AudioConverter will call ProvideInput() to fetch from |current_data_|.
    current_input_bus_ = &input_bus;
    converter_->Convert(audio_bus.get());
    DCHECK(!current_input_bus_);  // ProvideInput() called exactly once?

    sample_frames_in_ += input_params_.frames_per_buffer();
    sample_frames_out_ += audio_bus->frames();

    frame_input_->InsertAudio(std::move(audio_bus),
                              capture_time_of_first_converted_sample);
  }

  // Called on real-time audio thread.
  void OnSetFormat(const media::AudioParameters& params) override {
    if (input_params_.Equals(params))
      return;
    input_params_ = params;

    DVLOG(1) << "Setting up audio resampling: {"
             << input_params_.channels() << " channels, "
             << input_params_.sample_rate() << " Hz} --> {"
             << output_channels_ << " channels, "
             << output_sample_rate_ << " Hz}";
    const media::AudioParameters output_params(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::GuessChannelLayout(output_channels_), output_sample_rate_,
        output_sample_rate_ * input_params_.frames_per_buffer() /
            input_params_.sample_rate());
    converter_.reset(
        new media::AudioConverter(input_params_, output_params, false));
    converter_->AddInput(this);
    sample_frames_in_ = 0;
    sample_frames_out_ = 0;
  }

  // Called on real-time audio thread.
  double ProvideInput(media::AudioBus* audio_bus,
                      uint32_t frames_delayed) override {
    DCHECK(current_input_bus_);
    current_input_bus_->CopyTo(audio_bus);
    current_input_bus_ = nullptr;
    return 1.0;
  }

 private:
  const blink::WebMediaStreamTrack track_;
  const int output_channels_;
  const int output_sample_rate_;

  // This must be set before the real-time audio thread starts calling OnData(),
  // and remain unchanged until after the thread will stop calling OnData().
  scoped_refptr<media::cast::AudioFrameInput> frame_input_;

  // These members are accessed on the real-time audio time only.
  media::AudioParameters input_params_;
  std::unique_ptr<media::AudioConverter> converter_;
  const media::AudioBus* current_input_bus_;
  int64_t sample_frames_in_;
  int64_t sample_frames_out_;

  DISALLOW_COPY_AND_ASSIGN(CastAudioSink);
};

bool CastRtpStream::IsHardwareVP8EncodingSupported() {
  // Query for hardware VP8 encoder support.
  const std::vector<media::VideoEncodeAccelerator::SupportedProfile>
      vea_profiles = content::GetSupportedVideoEncodeAcceleratorProfiles();
  for (const auto& vea_profile : vea_profiles) {
    if (vea_profile.profile >= media::VP8PROFILE_MIN &&
        vea_profile.profile <= media::VP8PROFILE_MAX) {
      return true;
    }
  }
  return false;
}

bool CastRtpStream::IsHardwareH264EncodingSupported() {
// Query for hardware H.264 encoder support.
//
// TODO(miu): Look into why H.264 hardware encoder on MacOS is broken.
// http://crbug.com/596674
// TODO(emircan): Look into HW encoder initialization issues on Win.
// https://crbug.com/636064
#if !defined(OS_MACOSX) && !defined(OS_WIN)
  const std::vector<media::VideoEncodeAccelerator::SupportedProfile>
      vea_profiles = content::GetSupportedVideoEncodeAcceleratorProfiles();
  for (const auto& vea_profile : vea_profiles) {
    if (vea_profile.profile >= media::H264PROFILE_MIN &&
        vea_profile.profile <= media::H264PROFILE_MAX) {
      return true;
    }
  }
#endif  // !defined(OS_MACOSX) && !defined(OS_WIN)
  return false;
}

CastRtpStream::CastRtpStream(const blink::WebMediaStreamTrack& track,
                             const scoped_refptr<CastSession>& session)
    : track_(track),
      cast_session_(session),
      is_audio_(track_.Source().GetType() ==
                blink::WebMediaStreamSource::kTypeAudio) {}

CastRtpStream::CastRtpStream(bool is_audio,
                             const scoped_refptr<CastSession>& session)
    : cast_session_(session), is_audio_(is_audio) {}

CastRtpStream::~CastRtpStream() {
  Stop();
}

std::vector<FrameSenderConfig> CastRtpStream::GetSupportedConfigs() {
  if (is_audio_)
    return SupportedAudioConfigs(track_.IsNull());
  else
    return SupportedVideoConfigs(track_.IsNull());
}

void CastRtpStream::Start(int32_t stream_id,
                          const FrameSenderConfig& config,
                          const base::Closure& start_callback,
                          const base::Closure& stop_callback,
                          const ErrorCallback& error_callback) {
  DCHECK(!start_callback.is_null());
  DCHECK(!stop_callback.is_null());
  DCHECK(!error_callback.is_null());

  DVLOG(1) << "CastRtpStream::Start = " << (is_audio_ ? "audio" : "video");
  stop_callback_ = stop_callback;
  error_callback_ = error_callback;

  if (track_.IsNull()) {
    cast_session_->StartRemotingStream(
        stream_id, config, base::Bind(&CastRtpStream::DidEncounterError,
                                      weak_factory_.GetWeakPtr()));
  } else if (is_audio_) {
    // In case of error we have to go through DidEncounterError() to stop
    // the streaming after reporting the error.
    audio_sink_.reset(
        new CastAudioSink(track_, config.channels, config.rtp_timebase));
    cast_session_->StartAudio(
        config,
        base::Bind(&CastAudioSink::AddToTrack, audio_sink_->AsWeakPtr()),
        base::Bind(&CastRtpStream::DidEncounterError,
                   weak_factory_.GetWeakPtr()));
  } else {
    // See the code for audio above for explanation of callbacks.
    video_sink_.reset(new CastVideoSink(
        track_,
        media::BindToCurrentLoop(base::Bind(&CastRtpStream::DidEncounterError,
                                            weak_factory_.GetWeakPtr()))));
    cast_session_->StartVideo(
        config, base::Bind(&CastVideoSink::AddToTrack, video_sink_->AsWeakPtr(),
                           !config.aes_key.empty()),
        base::Bind(&CastRtpStream::DidEncounterError,
                   weak_factory_.GetWeakPtr()));
  }
  start_callback.Run();
}

void CastRtpStream::Stop() {
  DVLOG(1) << "CastRtpStream::Stop = " << (is_audio_ ? "audio" : "video");
  if (stop_callback_.is_null())
    return;  // Already stopped.
  weak_factory_.InvalidateWeakPtrs();
  error_callback_.Reset();
  audio_sink_.reset();
  video_sink_.reset();
  std::move(stop_callback_).Run();
}

void CastRtpStream::ToggleLogging(bool enable) {
  DVLOG(1) << "CastRtpStream::ToggleLogging(" << enable
           << ") = " << (is_audio_ ? "audio" : "video");
  cast_session_->ToggleLogging(is_audio_, enable);
}

void CastRtpStream::GetRawEvents(
    const base::Callback<void(std::unique_ptr<base::Value>)>& callback,
    const std::string& extra_data) {
  DVLOG(1) << "CastRtpStream::GetRawEvents = "
           << (is_audio_ ? "audio" : "video");
  cast_session_->GetEventLogsAndReset(is_audio_, extra_data, callback);
}

void CastRtpStream::GetStats(
    const base::Callback<void(std::unique_ptr<base::DictionaryValue>)>&
        callback) {
  DVLOG(1) << "CastRtpStream::GetStats = " << (is_audio_ ? "audio" : "video");
  cast_session_->GetStatsAndReset(is_audio_, callback);
}

void CastRtpStream::DidEncounterError(const std::string& message) {
  DCHECK(content::RenderThread::Get());
  DVLOG(1) << "CastRtpStream::DidEncounterError(" << message
           << ") = " << (is_audio_ ? "audio" : "video");
  // Save the WeakPtr first because the error callback might delete this object.
  base::WeakPtr<CastRtpStream> ptr = weak_factory_.GetWeakPtr();
  error_callback_.Run(message);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&CastRtpStream::Stop, ptr));
}
