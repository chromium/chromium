// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/audio_source_fetcher_impl.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/services/speech/speech_recognition_recognizer_impl.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_mixer.h"
#include "media/base/limits.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/audio/public/cpp/device_factory.h"

namespace speech {

namespace {

// Buffer size should be 100ms.
constexpr int kServerBasedRecognitionAudioSampleRate = 16000;
constexpr base::TimeDelta kServerBasedRecognitionAudioBufferSize =
    base::Milliseconds(100);

constexpr char kServerBasedRecognitionSessionLength[] =
    "Ash.SpeechRecognitionSessionLength.ServerBased";
constexpr char kOnDeviceRecognitionSessionLength[] =
    "Ash.SpeechRecognitionSessionLength.OnDevice";

}  // namespace

AudioSourceFetcherImpl::AudioSourceFetcherImpl(
    std::unique_ptr<AudioSourceConsumer> audio_consumer,
    bool is_multi_channel_supported,
    bool is_server_based)
    : audio_consumer_(std::move(audio_consumer)),
      is_started_(false),
      is_multi_channel_supported_(is_multi_channel_supported),
      is_server_based_(is_server_based) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AudioSourceFetcherImpl::~AudioSourceFetcherImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
  const auto* session_length_metric_name =
      is_server_based_ ? kServerBasedRecognitionSessionLength
                       : kOnDeviceRecognitionSessionLength;
  base::UmaHistogramLongTimes100(session_length_metric_name, audio_length_);
}

void AudioSourceFetcherImpl::Create(
    mojo::PendingReceiver<media::mojom::AudioSourceFetcher> receiver,
    std::unique_ptr<AudioSourceConsumer> recognition_recognizer,
    bool is_multi_channel_supported,
    bool is_server_based) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<AudioSourceFetcherImpl>(
                                  std::move(recognition_recognizer),
                                  is_multi_channel_supported, is_server_based),
                              std::move(receiver));
}

void AudioSourceFetcherImpl::Start(
    mojo::PendingRemote<media::mojom::AudioStreamFactory> stream_factory,
    const std::string& device_id,
    const ::media::AudioParameters& audio_parameters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If we've already started fetching audio from this device with these params,
  // return early. Otherwise start over and reset.
  if (is_started_) {
    if (device_id == device_id_ && audio_parameters.Equals(audio_parameters_)) {
      LOG(ERROR)
          << "AudioSourceFetcher was already running, and was asked to restart "
             "with the same device ID and audio parameters. Doing nothing.";
      return;
    } else {
      Stop();
    }
  }

  device_id_ = device_id;
  audio_parameters_ = audio_parameters;

  // Resample only if the recognizer is server based and the device's sample
  // rate is > 16khz.
  if (is_server_based_ && audio_parameters_.sample_rate() >
                              kServerBasedRecognitionAudioSampleRate) {
    server_based_recognition_params_ = media::AudioParameters(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        is_multi_channel_supported_ ? audio_parameters_.channel_layout_config()
                                    : media::ChannelLayoutConfig::Mono(),
        kServerBasedRecognitionAudioSampleRate,
        media::AudioTimestampHelper::TimeToFrames(
            kServerBasedRecognitionAudioBufferSize,
            kServerBasedRecognitionAudioSampleRate));

    // Bind to current loop to ensure the `ConvertingAudioFifo::OutputCallback`
    // and `ConvertingAudioFifo::Push` to be called on same thread.
    converter_ = std::make_unique<media::ConvertingAudioFifo>(
        audio_parameters_, server_based_recognition_params_.value());
    resample_callback_ = base::BindPostTaskToCurrentDefault(
        base::BindRepeating(&AudioSourceFetcherImpl::SendAudioToResample,
                            weak_factory_.GetWeakPtr()));
  }

  auto audio_log_remote = VLOG_IS_ON(1)
                              ? audio_log_receiver_.BindNewPipeAndPassRemote()
                              : mojo::NullRemote();
  audio_capturer_source_ = audio::CreateInputDevice(
      std::move(stream_factory), device_id_,
      audio::DeadStreamDetection::kEnabled, std::move(audio_log_remote));
  DCHECK(audio_capturer_source_);

  send_error_callback_ = base::BindPostTaskToCurrentDefault(base::BindRepeating(
      &AudioSourceFetcherImpl::SendError, weak_factory_.GetWeakPtr()));

  // TODO(crbug.com/40753481): Check implementation / sandbox policy on Mac and
  // Windows.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  is_started_ = true;
  // Initialize the AudioCapturerSource with |this| as the CaptureCallback,
  // get the parameters for the device ID, then start audio capture.
  send_audio_callback_ = base::BindPostTaskToCurrentDefault(base::BindRepeating(
      &AudioSourceFetcherImpl::SendAudioToSpeechRecognitionService,
      weak_factory_.GetWeakPtr()));
  GetAudioCapturerSource()->Initialize(audio_parameters_, this);
  GetAudioCapturerSource()->Start();
#endif
}

void AudioSourceFetcherImpl::DrainConverterOutput() {
  while (converter_->HasOutput()) {
    OnAudioFinishedConvert(converter_->PeekOutput());
    converter_->PopOutput();
  }
}

void AudioSourceFetcherImpl::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetAudioCapturerSource()) {
    GetAudioCapturerSource()->Stop();
    audio_capturer_source_.reset();
  }
  is_started_ = false;
  if (converter_) {
    // If converter is not null, flush remaining frames.
    converter_->Flush();
    DrainConverterOutput();
    converter_.reset();
  }
  send_audio_callback_.Reset();

  // Ensure `SendAudioEndToSpeechRecognitionService` is executed after
  // `SendAudioToSpeechRecognitionService`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioSourceFetcherImpl::SendAudioEndToSpeechRecognitionService,
          weak_factory_.GetWeakPtr()));
}

void AudioSourceFetcherImpl::Capture(const media::AudioBus* audio_source,
                                     base::TimeTicks audio_capture_time,
                                     const media::AudioGlitchInfo& glitch_info,
                                     double volume,
                                     bool key_pressed) {
  audio_length_ += media::AudioTimestampHelper::FramesToTime(
      audio_source->frames(), audio_parameters_.sample_rate());

  if (converter_) {
    // Send the audio callback to the main thread to resample.
    std::unique_ptr<media::AudioBus> input =
        media::AudioBus::Create(audio_parameters_);
    audio_source->CopyTo(input.get());
    resample_callback_.Run(std::move(input));
  } else {
    // Send the audio callback to the main thread.
    send_audio_callback_.Run(ConvertToAudioDataS16(
        *audio_source, audio_parameters_.sample_rate(),
        audio_parameters_.channel_layout(), is_multi_channel_supported_));
  }
}

void AudioSourceFetcherImpl::OnCaptureError(
    media::AudioCapturerSource::ErrorCode code,
    const std::string& message) {
  LOG(ERROR) << "Audio Capture Error" << message;
  send_error_callback_.Run();
}

void AudioSourceFetcherImpl::SendAudioToSpeechRecognitionService(
    media::mojom::AudioDataS16Ptr buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  audio_consumer_->AddAudio(std::move(buffer));
}

void AudioSourceFetcherImpl::SendAudioToResample(
    std::unique_ptr<media::AudioBus> audio_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `converter_` will be null if Stop() has been called.
  if (converter_) {
    converter_->Push(std::move(audio_data));
    DrainConverterOutput();
  }
}

void AudioSourceFetcherImpl::SendError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  audio_consumer_->OnAudioCaptureError();
}

media::AudioCapturerSource* AudioSourceFetcherImpl::GetAudioCapturerSource() {
  return audio_capturer_source_for_tests_
             ? audio_capturer_source_for_tests_.get()
             : audio_capturer_source_.get();
}

void AudioSourceFetcherImpl::OnCreated(const media::AudioParameters& params,
                                       const std::string& device_id) {
  VLOG(1) << "Created fetcher for device " << device_id << " with params "
          << params.AsHumanReadableString();
}

void AudioSourceFetcherImpl::OnStarted() {
  VLOG(1) << "OnStarted for " << device_id_;
}
void AudioSourceFetcherImpl::OnStopped() {
  VLOG(1) << "OnStopped for " << device_id_;
}
void AudioSourceFetcherImpl::OnClosed() {
  VLOG(1) << "OnClosed for " << device_id_;
}
void AudioSourceFetcherImpl::OnError() {
  VLOG(1) << "OnError for " << device_id_;
}
void AudioSourceFetcherImpl::OnSetVolume(double volume) {
  VLOG(1) << "Set volume for " << device_id_ << " to " << volume;
}
void AudioSourceFetcherImpl::OnLogMessage(const std::string& message) {
  VLOG(1) << "Log Messages for " << device_id_ << ": " << message;
}
void AudioSourceFetcherImpl::OnProcessingStateChanged(
    const std::string& message) {
  VLOG(1) << "Processing State Changed for " << device_id_ << ": " << message;
}

void AudioSourceFetcherImpl::OnAudioFinishedConvert(
    const media::AudioBus* output_bus) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(output_bus && send_audio_callback_);
  send_audio_callback_.Run(ConvertToAudioDataS16(
      *output_bus, server_based_recognition_params_->sample_rate(),
      server_based_recognition_params_->channel_layout(),
      is_multi_channel_supported_));
}

void AudioSourceFetcherImpl::SendAudioEndToSpeechRecognitionService() {
  audio_consumer_->OnAudioCaptureEnd();
}

}  // namespace speech
