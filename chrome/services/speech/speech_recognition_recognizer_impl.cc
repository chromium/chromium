// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/speech_recognition_recognizer_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "chrome/services/speech/soda/soda_client.h"
#include "google_apis/google_api_keys.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_sample_types.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/mojo/common/media_type_converters.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace speech {

constexpr char kInvalidAudioDataError[] = "Invalid audio data received.";

namespace {

// Callback executed by the SODA library on a speech recognition event. The
// callback handle is a void pointer to the SpeechRecognitionRecognizerImpl that
// owns the SODA instance. SpeechRecognitionRecognizerImpl owns the SodaClient
// which owns the instance of SODA and their sequential destruction order
// ensures that this callback will never be called with an invalid callback
// handle to the SpeechRecognitionRecognizerImpl.
void RecognitionCallback(const char* result,
                         const bool is_final,
                         void* callback_handle) {
  DCHECK(callback_handle);
  static_cast<SpeechRecognitionRecognizerImpl*>(callback_handle)
      ->recognition_event_callback()
      .Run(std::string(result), is_final);
}

}  // namespace

SpeechRecognitionRecognizerImpl::~SpeechRecognitionRecognizerImpl() = default;

void SpeechRecognitionRecognizerImpl::Create(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> remote,
    base::WeakPtr<SpeechRecognitionServiceImpl> speech_recognition_service_impl,
    const base::FilePath& binary_path,
    const base::FilePath& config_path) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<SpeechRecognitionRecognizerImpl>(
          std::move(remote), std::move(speech_recognition_service_impl),
          binary_path, config_path),
      std::move(receiver));
}

bool SpeechRecognitionRecognizerImpl::IsMultichannelSupported() {
  if (base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption)) {
    return true;
  } else {
    return false;
  }
}

void SpeechRecognitionRecognizerImpl::OnRecognitionEvent(
    const std::string& result,
    const bool is_final) {
  client_remote_->OnSpeechRecognitionRecognitionEvent(
      media::mojom::SpeechRecognitionResult::New(result, is_final));
}

SpeechRecognitionRecognizerImpl::SpeechRecognitionRecognizerImpl(
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> remote,
    base::WeakPtr<SpeechRecognitionServiceImpl> speech_recognition_service_impl,
    const base::FilePath& binary_path,
    const base::FilePath& config_path)
    : client_remote_(std::move(remote)), config_path_(config_path) {
  recognition_event_callback_ = media::BindToCurrentLoop(
      base::Bind(&SpeechRecognitionRecognizerImpl::OnRecognitionEvent,
                 weak_factory_.GetWeakPtr()));
  enable_soda_ = base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption);
  if (enable_soda_) {
    DCHECK(base::PathExists(binary_path));
    soda_client_ = std::make_unique<soda::SodaClient>(binary_path);
  } else {
    cloud_client_ = std::make_unique<CloudSpeechRecognitionClient>(
        recognition_event_callback(),
        std::move(speech_recognition_service_impl));
  }
}

void SpeechRecognitionRecognizerImpl::SendAudioToSpeechRecognitionService(
    media::mojom::AudioDataS16Ptr buffer) {
  int channel_count = buffer->channel_count;
  int frame_count = buffer->frame_count;
  int sample_rate = buffer->sample_rate;
  size_t num_samples = 0;
  size_t buffer_size = 0;

  // Verify the channel count.
  if (channel_count <= 0 || channel_count > media::limits::kMaxChannels) {
    mojo::ReportBadMessage(kInvalidAudioDataError);
    return;
  }

  // Verify and calculate the number of samples.
  if (sample_rate <= 0 || frame_count <= 0 ||
      !base::CheckMul(frame_count, channel_count).AssignIfValid(&num_samples) ||
      num_samples != buffer->data.size()) {
    mojo::ReportBadMessage(kInvalidAudioDataError);
    return;
  }

  // Verify and calculate the buffer size.
  if (!base::CheckMul(buffer->data.size(), sizeof(buffer->data[0]))
           .AssignIfValid(&buffer_size)) {
    mojo::ReportBadMessage(kInvalidAudioDataError);
    return;
  }

  if (enable_soda_) {
    DCHECK(soda_client_);
    DCHECK(base::PathExists(config_path_));
    if (!soda_client_->IsInitialized() ||
        soda_client_->DidAudioPropertyChange(sample_rate, channel_count)) {
      // Initialize the SODA instance.
      auto api_key = google_apis::GetSodaAPIKey();
      std::string language_pack_directory = config_path_.AsUTF8Unsafe();
      SodaConfig config;
      config.channel_count = channel_count;
      config.sample_rate = sample_rate;
      config.language_pack_directory = language_pack_directory.c_str();
      config.callback = RecognitionCallback;
      config.callback_handle = this;
      config.api_key = api_key.c_str();
      soda_client_->Reset(config);
    }

    soda_client_->AddAudio(reinterpret_cast<char*>(buffer->data.data()),
                           buffer_size);
  } else {
    DCHECK(cloud_client_);
    if (!cloud_client_->IsInitialized() ||
        cloud_client_->DidAudioPropertyChange(sample_rate, channel_count)) {
      // Initialize the stream.
      CloudSpeechConfig config;
      config.sample_rate = sample_rate;
      config.channel_count = channel_count;
      config.language_code = "en-US";
      cloud_client_->Initialize(config);
    }

    cloud_client_->AddAudio(base::span<const char>(
        reinterpret_cast<char*>(buffer->data.data()), buffer_size));
  }
}

}  // namespace speech
