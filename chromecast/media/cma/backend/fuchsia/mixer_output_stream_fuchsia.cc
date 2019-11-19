// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/fuchsia/mixer_output_stream_fuchsia.h"

#include <fuchsia/media/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <zircon/syscalls.h>

#include "base/command_line.h"
#include "base/fuchsia/default_context.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/time/time.h"
#include "chromecast/base/chromecast_switches.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/media_switches.h"

namespace chromecast {
namespace media {

// Target period between Write() calls. It's used to calculate the value
// returned from OptimalWriteFramesCount().
constexpr base::TimeDelta kTargetWritePeriod =
    base::TimeDelta::FromMilliseconds(10);

// Same value as in MixerOutputStreamAlsa. Currently this value is used to
// simulate blocking Write() similar to ALSA's behavior, see comments in
// MixerOutputStreamFuchsia::Write().
constexpr int kMaxOutputBufferSizeFrames = 4096;

// Current AudioRenderer implementation allows only one buffer with id=0.
// TODO(sergeyu): Replace with an incrementing buffer id once AddPayloadBuffer()
// and RemovePayloadBuffer() are implemented properly in AudioRenderer.
const uint32_t kBufferId = 0;

// static
std::unique_ptr<MixerOutputStream> MixerOutputStream::Create() {
  return std::make_unique<MixerOutputStreamFuchsia>();
}

MixerOutputStreamFuchsia::MixerOutputStreamFuchsia() = default;
MixerOutputStreamFuchsia::~MixerOutputStreamFuchsia() = default;

bool MixerOutputStreamFuchsia::Start(int requested_sample_rate, int channels) {
  DCHECK(!audio_renderer_);
  DCHECK(reference_time_.is_null());

  sample_rate_ = requested_sample_rate;
  channels_ = channels;
  target_packet_size_ = ::media::AudioTimestampHelper::TimeToFrames(
      kTargetWritePeriod, sample_rate_);

  // Connect |audio_renderer_|.
  fuchsia::media::AudioPtr audio_server =
      base::fuchsia::ComponentContextForCurrentProcess()
          ->svc()
          ->Connect<fuchsia::media::Audio>();
  audio_server->CreateAudioRenderer(audio_renderer_.NewRequest());
  audio_renderer_.set_error_handler(
      [this](zx_status_t status) { this->OnRendererError(status); });

  // Configure the renderer.
  fuchsia::media::AudioStreamType format;
  format.sample_format = fuchsia::media::AudioSampleFormat::FLOAT;
  format.channels = channels_;
  format.frames_per_second = sample_rate_;
  audio_renderer_->SetPcmStreamType(std::move(format));

  // Use number of samples to specify media position.
  audio_renderer_->SetPtsUnits(sample_rate_, 1);

  audio_renderer_->EnableMinLeadTimeEvents(true);
  audio_renderer_.events().OnMinLeadTimeChanged =
      fit::bind_member(this, &MixerOutputStreamFuchsia::OnMinLeadTimeChanged);

  return true;
}

int MixerOutputStreamFuchsia::GetNumChannels() {
  return channels_;
}

int MixerOutputStreamFuchsia::GetSampleRate() {
  return sample_rate_;
}

MediaPipelineBackend::AudioDecoder::RenderingDelay
MixerOutputStreamFuchsia::GetRenderingDelay() {
  if (reference_time_.is_null())
    return MediaPipelineBackend::AudioDecoder::RenderingDelay();

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delay = GetCurrentStreamTime() - now;
  return MediaPipelineBackend::AudioDecoder::RenderingDelay(
      /*delay_microseconds=*/delay.InMicroseconds(),
      /*timestamp_microseconds=*/(now - base::TimeTicks()).InMicroseconds());
}

int MixerOutputStreamFuchsia::OptimalWriteFramesCount() {
  return target_packet_size_;
}

bool MixerOutputStreamFuchsia::Write(const float* data,
                                     int data_size,
                                     bool* out_playback_interrupted) {
  if (!audio_renderer_)
    return false;

  DCHECK_EQ(data_size % channels_, 0);

  // Allocate payload buffer if necessary.
  if (!payload_buffer_.IsValid() && !InitializePayloadBuffer())
    return false;

  // If Write() was called for the current playback position then assume that
  // playback was interrupted.
  auto now = base::TimeTicks::Now();
  bool playback_interrupted = !reference_time_.is_null() &&
                              now >= (GetCurrentStreamTime() - min_lead_time_);
  if (out_playback_interrupted)
    *out_playback_interrupted = playback_interrupted;

  // Reset playback position if playback was interrupted.
  if (playback_interrupted)
    reference_time_ = base::TimeTicks();

  size_t packet_size = data_size * sizeof(float);
  if (payload_buffer_pos_ + packet_size > payload_buffer_.size()) {
    payload_buffer_pos_ = 0;
  }

  DCHECK_LE(payload_buffer_pos_ + data_size, payload_buffer_.size());
  memcpy(reinterpret_cast<uint8_t*>(payload_buffer_.memory()) +
             payload_buffer_pos_,
         data, packet_size);

  // Send a new packet.
  fuchsia::media::StreamPacket packet;
  packet.pts = stream_position_samples_;
  packet.payload_buffer_id = kBufferId;
  packet.payload_offset = payload_buffer_pos_;
  packet.payload_size = packet_size;
  packet.flags = 0;
  audio_renderer_->SendPacketNoReply(std::move(packet));

  // Update stream position.
  int frames = data_size / channels_;
  stream_position_samples_ += frames;
  payload_buffer_pos_ += packet_size;

  if (reference_time_.is_null()) {
    reference_time_ = now + min_lead_time_;
    audio_renderer_->PlayNoReply(reference_time_.ToZxTime(),
                                 stream_position_samples_ - frames);
  } else {
    // Block the thread to limit amount of buffered data. Currently
    // MixerOutputStreamAlsa uses blocking Write() and StreamMixer relies on
    // that behavior. Sleep() below replicates the same behavior on Fuchsia.
    // TODO(sergeyu): Refactor StreamMixer to work with non-blocking Write().
    base::TimeDelta max_buffer_duration =
        ::media::AudioTimestampHelper::FramesToTime(kMaxOutputBufferSizeFrames,
                                                    sample_rate_);
    base::TimeDelta current_buffer_duration =
        GetCurrentStreamTime() - min_lead_time_ - now;
    if (current_buffer_duration > max_buffer_duration) {
      base::PlatformThread::Sleep(current_buffer_duration -
                                  max_buffer_duration);
    }
  }

  return true;
}

void MixerOutputStreamFuchsia::Stop() {
  reference_time_ = base::TimeTicks();
  audio_renderer_.Unbind();
}

size_t MixerOutputStreamFuchsia::GetMinBufferSize() {
  // Ensure that |payload_buffer_| fits enough packets to cover |min_lead_time_|
  // and kMaxOutputBufferSizeFrames plus one extra packet.
  int min_packets = (::media::AudioTimestampHelper::TimeToFrames(min_lead_time_,
                                                                 sample_rate_) +
                     kMaxOutputBufferSizeFrames + target_packet_size_ - 1) /
                        target_packet_size_ +
                    1;
  return min_packets * target_packet_size_ * channels_ * sizeof(float);
}

bool MixerOutputStreamFuchsia::InitializePayloadBuffer() {
  size_t buffer_size = GetMinBufferSize();
  auto region = base::WritableSharedMemoryRegion::Create(buffer_size);
  payload_buffer_ = region.Map();
  if (!payload_buffer_.IsValid()) {
    LOG(WARNING) << "Failed to allocate VMO of size " << buffer_size;
    return false;
  }

  payload_buffer_pos_ = 0;
  audio_renderer_->AddPayloadBuffer(
      kBufferId, base::WritableSharedMemoryRegion::TakeHandleForSerialization(
                     std::move(region))
                     .PassPlatformHandle());

  return true;
}

base::TimeTicks MixerOutputStreamFuchsia::GetCurrentStreamTime() {
  DCHECK(!reference_time_.is_null());
  return reference_time_ + ::media::AudioTimestampHelper::FramesToTime(
                               stream_position_samples_, sample_rate_);
}

void MixerOutputStreamFuchsia::OnRendererError(zx_status_t status) {
  LOG(WARNING) << "AudioRenderer has failed with code " << status;
  Stop();
}

void MixerOutputStreamFuchsia::OnMinLeadTimeChanged(int64_t min_lead_time) {
  min_lead_time_ = base::TimeDelta::FromNanoseconds(min_lead_time);

  // When min_lead_time_ increases we may need to reallocate |payload_buffer_|.
  // Code below just unmaps the current buffer. The new buffer will be allocated
  // lated in PumpSamples(). This is necessary because VMO allocation may fail
  // and it's not possible to report that error here - OnMinLeadTimeChanged()
  // may be invoked before Start().
  if (payload_buffer_.IsValid() &&
      GetMinBufferSize() > payload_buffer_.size()) {
    payload_buffer_ = {};
  }
}

}  // namespace media
}  // namespace chromecast
