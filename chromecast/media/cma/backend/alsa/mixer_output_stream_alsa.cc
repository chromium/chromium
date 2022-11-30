// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/mixer_output_stream_alsa.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/media/cma/backend/alsa/alsa_wrapper.h"
#include "media/base/audio_sample_types.h"
#include "media/base/media_switches.h"

#define RETURN_FALSE_ON_ERROR(snd_func, ...)                        \
  do {                                                              \
    int a_err = alsa_->snd_func(__VA_ARGS__);                       \
    if (a_err < 0) {                                                \
      LOG(ERROR) << #snd_func " error: " << alsa_->StrError(a_err); \
      return false;                                                 \
    }                                                               \
  } while (0)

#define RETURN_ERROR_CODE(snd_func, ...)                            \
  do {                                                              \
    int a_err = alsa_->snd_func(__VA_ARGS__);                       \
    if (a_err < 0) {                                                \
      LOG(ERROR) << #snd_func " error: " << alsa_->StrError(a_err); \
      return a_err;                                                 \
    }                                                               \
  } while (0)

#define CHECK_PCM_INITIALIZED()                                              \
  if (!pcm_ || !pcm_hw_params_) {                                            \
    LOG(WARNING) << __FUNCTION__ << "() called after failed initialization"; \
    return false;                                                            \
  }

namespace chromecast {
namespace media {

namespace {

template <class TargetSampleTypeTraits>
void ToFixedPoint(const float* input,
                  int frames,
                  typename TargetSampleTypeTraits::ValueType* dest_buffer) {
  for (int f = 0; f < frames; ++f) {
    dest_buffer[f] = TargetSampleTypeTraits::FromFloat(input[f]);
  }
}

void ToFixedPoint(const float* input,
                  int frames,
                  int bytes_per_sample,
                  uint8_t* dest_buffer) {
  switch (bytes_per_sample) {
    case 1:
      ToFixedPoint<::media::UnsignedInt8SampleTypeTraits>(
          input, frames, reinterpret_cast<uint8_t*>(dest_buffer));
      break;
    case 2:
      ToFixedPoint<::media::SignedInt16SampleTypeTraits>(
          input, frames, reinterpret_cast<int16_t*>(dest_buffer));
      break;
    case 4:
      ToFixedPoint<::media::SignedInt32SampleTypeTraits>(
          input, frames, reinterpret_cast<int32_t*>(dest_buffer));
      break;
    default:
      NOTREACHED() << "Unsupported bytes per sample encountered: "
                   << bytes_per_sample;
  }
}

constexpr int64_t kNoTimestamp = std::numeric_limits<int64_t>::min();

constexpr char kOutputDeviceDefaultName[] = "default";

constexpr bool kPcmRecoverIsSilent = false;
constexpr int kDefaultOutputBufferSizeFrames = 1024;

// A list of supported sample rates.
// TODO(jyw): move this up into chromecast/public for 1) documentation and
// 2) to help when implementing IsSampleRateSupported()
// clang-format off
constexpr int kSupportedSampleRates[] =
    { 8000, 11025, 12000,
     16000, 22050, 24000,
     32000, 44100, 48000,
     64000, 88200, 96000};
// clang-format on

// Arbitrary sample rate in Hz to mix all audio to when a new primary input has
// a sample rate that is not directly supported, and a better fallback sample
// rate cannot be determined. 48000 is the highest supported non-hi-res sample
// rate. 96000 is the highest supported hi-res sample rate.
constexpr unsigned int kFallbackSampleRate = 48000;
constexpr unsigned int kFallbackSampleRateHiRes = 96000;

// The snd_pcm_(hw|sw)_params_set_*_near families of functions will report what
// direction they adjusted the requested parameter in, but since we read the
// output param and then log the information, this module doesn't need to get
// the direction explicitly.
constexpr int* kAlsaDirDontCare = nullptr;

// The snd_pcm_resume function can return EAGAIN error code, so call should be
// retried. Below constants define retries params.
constexpr int kRestoreAfterSuspensionAttempts = 10;
constexpr base::TimeDelta kRestoreAfterSuspensionAttemptDelay =
    base::Milliseconds(20);

// These sample formats will be tried in order. 32 bit samples is ideal, but
// some devices do not support 32 bit samples.
constexpr snd_pcm_format_t kPreferredSampleFormats[] = {
    SND_PCM_FORMAT_FLOAT, SND_PCM_FORMAT_S32, SND_PCM_FORMAT_S16};

int64_t TimespecToMicroseconds(struct timespec time) {
  return static_cast<int64_t>(time.tv_sec) *
             base::Time::kMicrosecondsPerSecond +
         time.tv_nsec / 1000;
}

}  // namespace

// static
std::unique_ptr<MixerOutputStream> MixerOutputStream::Create() {
  return std::make_unique<MixerOutputStreamAlsa>();
}

MixerOutputStreamAlsa::MixerOutputStreamAlsa() {
  DefineAlsaParameters();
}

MixerOutputStreamAlsa::~MixerOutputStreamAlsa() {
  Stop();
}

void MixerOutputStreamAlsa::SetAlsaWrapperForTest(
    std::unique_ptr<AlsaWrapper> alsa) {
  DCHECK(!alsa_);
  alsa_ = std::move(alsa);
}

bool MixerOutputStreamAlsa::Start(int sample_rate, int channels) {
  if (!alsa_) {
    alsa_ = std::make_unique<AlsaWrapper>();
  }

  num_output_channels_ = channels;

  // Open PCM devices when Start() is called for the first time.
  if (!pcm_) {
    std::string device_name = kOutputDeviceDefaultName;
    if (base::CommandLine::InitializedForCurrentProcess() &&
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kAlsaOutputDevice)) {
      device_name = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAlsaOutputDevice);
    }

    RETURN_FALSE_ON_ERROR(PcmOpen, &pcm_, device_name.c_str(),
                          SND_PCM_STREAM_PLAYBACK, 0);
    LOG(INFO) << "snd_pcm_open: handle=" << pcm_;
  }

  // Some OEM-developed Cast for Audio devices don't accurately report their
  // support for different output formats, so this tries 32-bit output and then
  // 16-bit output if that fails.
  //
  // TODO(cleichner): Replace this with more specific device introspection.
  // b/24747205
  int err = SetAlsaPlaybackParams(sample_rate);
  if (err < 0) {
    LOG(ERROR) << "Error setting ALSA playback parameters: "
               << alsa_->StrError(err);
    return false;
  }

  RETURN_FALSE_ON_ERROR(PcmPrepare, pcm_);
  RETURN_FALSE_ON_ERROR(PcmStatusMalloc, &pcm_status_);

  rendering_delay_.timestamp_microseconds = kNoTimestamp;
  rendering_delay_.delay_microseconds = 0;
  first_write_ = true;

  return true;
}

int MixerOutputStreamAlsa::GetNumChannels() {
  return num_output_channels_;
}

int MixerOutputStreamAlsa::GetSampleRate() {
  return sample_rate_;
}

MediaPipelineBackend::AudioDecoder::RenderingDelay
MixerOutputStreamAlsa::GetRenderingDelay() {
  return rendering_delay_;
}

int MixerOutputStreamAlsa::OptimalWriteFramesCount() {
  CHECK_PCM_INITIALIZED();
  return alsa_period_size_;
}

bool MixerOutputStreamAlsa::Write(const float* data,
                                  int data_size,
                                  bool* out_playback_interrupted) {
  CHECK_PCM_INITIALIZED();
  *out_playback_interrupted = false;
  int frames = data_size / num_output_channels_;
  ssize_t bytes_per_sample = alsa_->PcmFormatSize(pcm_format_, 1);
  const uint8_t* output_data;
  if (pcm_format_ == SND_PCM_FORMAT_FLOAT) {
    output_data = reinterpret_cast<const uint8_t*>(data);
  } else {
    // Resize interleaved if necessary.
    size_t output_data_size = data_size * bytes_per_sample;
    if (output_buffer_.size() < output_data_size) {
      output_buffer_.resize(output_data_size);
    }
    ToFixedPoint(data, data_size, bytes_per_sample, output_buffer_.data());
    output_data = output_buffer_.data();
  }

  // If the PCM has been drained it will be in SND_PCM_STATE_SETUP and need
  // to be prepared in order for playback to work.
  if (alsa_->PcmState(pcm_) == SND_PCM_STATE_SETUP) {
    RETURN_FALSE_ON_ERROR(PcmPrepare, pcm_);
  }

  int frames_left = frames;
  while (frames_left) {
    int frames_or_error;
    while ((frames_or_error =
                alsa_->PcmWritei(pcm_, output_data, frames_left)) < 0) {
      if (!first_write_) {
        *out_playback_interrupted = true;
      }
      if (frames_or_error == -EBADFD &&
          MaybeRecoverDeviceFromSuspendedState()) {
        // Write data again, if recovered.
        continue;
      }
      RETURN_FALSE_ON_ERROR(PcmRecover, pcm_, frames_or_error,
                            kPcmRecoverIsSilent);
    }
    frames_left -= frames_or_error;
    DCHECK_GE(frames_left, 0);
    output_data += frames_or_error * num_output_channels_ * bytes_per_sample;
  }
  first_write_ = false;
  UpdateRenderingDelay();

  return true;
}

void MixerOutputStreamAlsa::Stop() {
  if (alsa_) {
    alsa_->PcmStatusFree(pcm_status_);
    alsa_->PcmHwParamsFree(pcm_hw_params_);
  }

  pcm_status_ = nullptr;
  pcm_hw_params_ = nullptr;

  if (!pcm_) {
    return;
  }

  // If |pcm_| is RUNNING, drain all pending data.
  if (alsa_->PcmState(pcm_) == SND_PCM_STATE_RUNNING) {
    int err = alsa_->PcmDrain(pcm_);
    if (err < 0) {
      LOG(ERROR) << "snd_pcm_drain error: " << alsa_->StrError(err);
    }
  } else {
    int err = alsa_->PcmDrop(pcm_);
    if (err < 0) {
      LOG(ERROR) << "snd_pcm_drop error: " << alsa_->StrError(err);
    }
  }

  LOG(INFO) << "snd_pcm_close: handle=" << pcm_;
  int err = alsa_->PcmClose(pcm_);
  if (err < 0) {
    LOG(ERROR) << "snd_pcm_close error, leaking handle: "
               << alsa_->StrError(err);
  }
  pcm_ = nullptr;
}

int MixerOutputStreamAlsa::SetAlsaPlaybackParams(int requested_sample_rate) {
  int err = 0;
  // Set hardware parameters.
  DCHECK(pcm_);
  DCHECK(!pcm_hw_params_);
  RETURN_ERROR_CODE(PcmHwParamsMalloc, &pcm_hw_params_);
  RETURN_ERROR_CODE(PcmHwParamsAny, pcm_, pcm_hw_params_);
  RETURN_ERROR_CODE(PcmHwParamsSetAccess, pcm_, pcm_hw_params_,
                    SND_PCM_ACCESS_RW_INTERLEAVED);
  if (pcm_format_ == SND_PCM_FORMAT_UNKNOWN) {
    for (const auto& pcm_format : kPreferredSampleFormats) {
      err = alsa_->PcmHwParamsTestFormat(pcm_, pcm_hw_params_, pcm_format);
      if (err < 0) {
        LOG(WARNING) << "PcmHwParamsTestFormat: " << alsa_->StrError(err);
      } else {
        pcm_format_ = pcm_format;
        break;
      }
    }
    if (pcm_format_ == SND_PCM_FORMAT_UNKNOWN) {
      LOG(ERROR) << "Could not find a valid PCM format. Running "
                 << "/bin/alsa_api_test may be instructive.";
      return err;
    }
  }

  RETURN_ERROR_CODE(PcmHwParamsSetFormat, pcm_, pcm_hw_params_, pcm_format_);
  RETURN_ERROR_CODE(PcmHwParamsSetChannels, pcm_, pcm_hw_params_,
                    num_output_channels_);

  // Set output rate, allow resampling with a warning if the device doesn't
  // support the rate natively.
  RETURN_ERROR_CODE(PcmHwParamsSetRateResample, pcm_, pcm_hw_params_,
                    false /* Don't allow resampling. */);

  unsigned int new_sample_rate = DetermineOutputRate(requested_sample_rate);
  RETURN_ERROR_CODE(PcmHwParamsSetRateNear, pcm_, pcm_hw_params_,
                    &new_sample_rate, kAlsaDirDontCare);
  if (requested_sample_rate != static_cast<int>(new_sample_rate)) {
    LOG(WARNING) << "Requested sample rate (" << requested_sample_rate
                 << " Hz) does not match the actual sample rate ("
                 << new_sample_rate
                 << " Hz). This may lead to lower audio quality.";
  }
  LOG(INFO) << "Sample rate changed from " << sample_rate_ << " to "
            << new_sample_rate;
  sample_rate_ = static_cast<int>(new_sample_rate);

  snd_pcm_uframes_t requested_buffer_size = alsa_buffer_size_;
  RETURN_ERROR_CODE(PcmHwParamsSetBufferSizeNear, pcm_, pcm_hw_params_,
                    &alsa_buffer_size_);
  if (requested_buffer_size != alsa_buffer_size_) {
    LOG(WARNING) << "Requested buffer size (" << requested_buffer_size
                 << " frames) does not match the actual buffer size ("
                 << alsa_buffer_size_
                 << " frames). This may lead to an increase in "
                    "either audio latency or audio underruns.";

    if (alsa_period_size_ >= alsa_buffer_size_) {
      snd_pcm_uframes_t new_period_size = alsa_buffer_size_ / 2;
      LOG(DFATAL) << "Configured period size (" << alsa_period_size_
                  << ") is >= actual buffer size (" << alsa_buffer_size_
                  << "); reducing to " << new_period_size;
      alsa_period_size_ = new_period_size;
    }
    // Scale the start threshold and avail_min based on the new buffer size.
    float original_buffer_size = static_cast<float>(requested_buffer_size);
    float avail_min_ratio = original_buffer_size / alsa_avail_min_;
    alsa_avail_min_ = alsa_buffer_size_ / avail_min_ratio;
    float start_threshold_ratio = original_buffer_size / alsa_start_threshold_;
    alsa_start_threshold_ = alsa_buffer_size_ / start_threshold_ratio;
  }

  snd_pcm_uframes_t requested_period_size = alsa_period_size_;
  RETURN_ERROR_CODE(PcmHwParamsSetPeriodSizeNear, pcm_, pcm_hw_params_,
                    &alsa_period_size_, kAlsaDirDontCare);
  if (requested_period_size != alsa_period_size_) {
    LOG(WARNING) << "Requested period size (" << requested_period_size
                 << " frames) does not match the actual period size ("
                 << alsa_period_size_
                 << " frames). This may lead to an increase in "
                    "CPU usage or an increase in audio latency.";
  }
  RETURN_ERROR_CODE(PcmHwParams, pcm_, pcm_hw_params_);

  // Set software parameters.
  snd_pcm_sw_params_t* swparams;
  RETURN_ERROR_CODE(PcmSwParamsMalloc, &swparams);
  RETURN_ERROR_CODE(PcmSwParamsCurrent, pcm_, swparams);
  RETURN_ERROR_CODE(PcmSwParamsSetStartThreshold, pcm_, swparams,
                    alsa_start_threshold_);
  if (alsa_start_threshold_ > alsa_buffer_size_) {
    LOG(ERROR) << "Requested start threshold (" << alsa_start_threshold_
               << " frames) is larger than the buffer size ("
               << alsa_buffer_size_
               << " frames). Audio playback will not start.";
  }

  RETURN_ERROR_CODE(PcmSwParamsSetAvailMin, pcm_, swparams, alsa_avail_min_);
  RETURN_ERROR_CODE(PcmSwParamsSetTstampMode, pcm_, swparams,
                    SND_PCM_TSTAMP_ENABLE);
  RETURN_ERROR_CODE(PcmSwParamsSetTstampType, pcm_, swparams,
                    kAlsaTstampTypeMonotonicRaw);
  err = alsa_->PcmSwParams(pcm_, swparams);
  alsa_->PcmSwParamsFree(swparams);
  return err;
}

void MixerOutputStreamAlsa::DefineAlsaParameters() {
  // Get the ALSA output configuration from the command line.

  if (base::CommandLine::InitializedForCurrentProcess()) {
    alsa_buffer_size_ = GetSwitchValueNonNegativeInt(
        switches::kAlsaOutputBufferSize, kDefaultOutputBufferSizeFrames);
    alsa_period_size_ = GetSwitchValueNonNegativeInt(
        switches::kAlsaOutputPeriodSize, alsa_buffer_size_ / 2);
  } else {
    alsa_buffer_size_ = kDefaultOutputBufferSizeFrames;
    alsa_period_size_ = alsa_buffer_size_ / 2;
  }

  if (alsa_period_size_ >= alsa_buffer_size_) {
    LOG(DFATAL) << "ALSA period size must be smaller than the buffer size";
    alsa_period_size_ = alsa_buffer_size_ / 2;
  }

  LOG(INFO) << "ALSA buffer = " << alsa_buffer_size_
            << ", period = " << alsa_period_size_;

  if (base::CommandLine::InitializedForCurrentProcess()) {
    alsa_start_threshold_ = GetSwitchValueNonNegativeInt(
        switches::kAlsaOutputStartThreshold,
        (alsa_buffer_size_ / alsa_period_size_) * alsa_period_size_);
  } else {
    alsa_start_threshold_ =
        (alsa_buffer_size_ / alsa_period_size_) * alsa_period_size_;
  }
  if (alsa_start_threshold_ > alsa_buffer_size_) {
    LOG(DFATAL) << "ALSA start threshold must be no larger than "
                << "the buffer size";
    alsa_start_threshold_ =
        (alsa_buffer_size_ / alsa_period_size_) * alsa_period_size_;
  }

  // By default, allow the transfer when at least period_size samples can be
  // processed.
  if (base::CommandLine::InitializedForCurrentProcess()) {
    alsa_avail_min_ = GetSwitchValueNonNegativeInt(
        switches::kAlsaOutputAvailMin, alsa_period_size_);
  } else {
    alsa_avail_min_ = alsa_period_size_;
  }
  if (alsa_avail_min_ > alsa_buffer_size_) {
    LOG(DFATAL) << "ALSA avail min must be no larger than the buffer size";
    alsa_avail_min_ = alsa_period_size_;
  }
}

int MixerOutputStreamAlsa::DetermineOutputRate(int requested_sample_rate) {
  unsigned int unsigned_output_sample_rate = requested_sample_rate;

  // Try the requested sample rate. If the ALSA driver doesn't know how to deal
  // with it, try the nearest supported sample rate instead. Lastly, try some
  // common sample rates as a fallback. Note that PcmHwParamsSetRateNear
  // doesn't always choose a rate that's actually near the given input sample
  // rate when the input sample rate is not supported.
  const int* kSupportedSampleRatesEnd =
      kSupportedSampleRates + std::size(kSupportedSampleRates);
  auto* nearest_sample_rate =
      std::min_element(kSupportedSampleRates, kSupportedSampleRatesEnd,
                       [requested_sample_rate](int r1, int r2) -> bool {
                         return abs(requested_sample_rate - r1) <
                                abs(requested_sample_rate - r2);
                       });
  // Resample audio with sample rates deemed to be too low (i.e.  below 32kHz)
  // because some common AV receivers don't support optical out at these
  // frequencies. See b/26385501
  unsigned int first_choice_sample_rate = requested_sample_rate;
  const unsigned int preferred_sample_rates[] = {
      first_choice_sample_rate, static_cast<unsigned int>(*nearest_sample_rate),
      kFallbackSampleRateHiRes, kFallbackSampleRate};
  int err;
  for (const auto& sample_rate : preferred_sample_rates) {
    err = alsa_->PcmHwParamsTestRate(pcm_, pcm_hw_params_, sample_rate,
                                     0 /* try exact rate */);
    if (err == 0) {
      unsigned_output_sample_rate = sample_rate;
      break;
    }
  }
  LOG_IF(ERROR, err != 0) << "Even the fallback sample rate isn't supported! "
                          << "Have you tried /bin/alsa_api_test on-device?";
  return unsigned_output_sample_rate;
}

void MixerOutputStreamAlsa::UpdateRenderingDelay() {
  if (alsa_->PcmStatus(pcm_, pcm_status_) != 0 ||
      alsa_->PcmStatusGetState(pcm_status_) != SND_PCM_STATE_RUNNING) {
    rendering_delay_.timestamp_microseconds = kNoTimestamp;
    rendering_delay_.delay_microseconds = 0;
    return;
  }

  snd_htimestamp_t status_timestamp = {};
  alsa_->PcmStatusGetHtstamp(pcm_status_, &status_timestamp);
  if (status_timestamp.tv_sec == 0 && status_timestamp.tv_nsec == 0) {
    // ALSA didn't actually give us a timestamp.
    rendering_delay_.timestamp_microseconds = kNoTimestamp;
    rendering_delay_.delay_microseconds = 0;
    return;
  }

  rendering_delay_.timestamp_microseconds =
      TimespecToMicroseconds(status_timestamp);
  snd_pcm_sframes_t delay_frames = alsa_->PcmStatusGetDelay(pcm_status_);
  rendering_delay_.delay_microseconds = static_cast<int64_t>(delay_frames) *
                                        base::Time::kMicrosecondsPerSecond /
                                        sample_rate_;
}

bool MixerOutputStreamAlsa::MaybeRecoverDeviceFromSuspendedState() {
  if (alsa_->PcmState(pcm_) != SND_PCM_STATE_SUSPENDED) {
    LOG(WARNING) << "Alsa output is not suspended";
    return false;
  }
  if (alsa_->PcmHwParamsCanResume(pcm_hw_params_)) {
    LOG(INFO) << "Trying to resume output";
    for (int attempt = 0; attempt < kRestoreAfterSuspensionAttempts;
         ++attempt) {
      int err = alsa_->PcmResume(pcm_);
      if (err == 0) {
        LOG(INFO) << "ALSA output is resumed from suspended state";
        return true;
      }
      if (err != -EAGAIN) {
        // If PcmResume failed or device doesn't support resume, try to use
        // PcmPrepare.
        err = alsa_->PcmPrepare(pcm_);
        LOG_IF(INFO, err == 0)
            << "ALSA output is recovered from suspended state";
        return err == 0;
      }
      base::PlatformThread::Sleep(kRestoreAfterSuspensionAttemptDelay);
    }
  }
  return false;
}

}  // namespace media
}  // namespace chromecast
