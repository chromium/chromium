// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/chrome_speech_recognition_client.h"

#include <utility>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/renderer/render_frame.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/channel_mixer.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"

// Get the list of blocked URLs defined by the Finch experiment parameter. These
// websites provide captions by default and thus do not require the live caption
// feature.
std::vector<std::string> GetBlockedURLs() {
  return base::SplitString(base::GetFieldTrialParamValueByFeature(
                               media::kLiveCaption, "blocked_websites"),
                           ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
}

ChromeSpeechRecognitionClient::ChromeSpeechRecognitionClient(
    content::RenderFrame* render_frame,
    media::SpeechRecognitionClient::OnReadyCallback callback)
    : render_frame_(render_frame),
      on_ready_callback_(std::move(callback)),
      blocked_urls_(GetBlockedURLs()) {
  initialize_callback_ = media::BindToCurrentLoop(base::BindRepeating(
      &ChromeSpeechRecognitionClient::Initialize, weak_factory_.GetWeakPtr()));

  send_audio_callback_ = media::BindToCurrentLoop(base::BindRepeating(
      &ChromeSpeechRecognitionClient::SendAudioToSpeechRecognitionService,
      weak_factory_.GetWeakPtr()));

  mojo::PendingReceiver<media::mojom::SpeechRecognitionClientBrowserInterface>
      speech_recognition_client_browser_interface_receiver =
          speech_recognition_client_browser_interface_
              .BindNewPipeAndPassReceiver();
  speech_recognition_client_browser_interface_
      ->BindSpeechRecognitionBrowserObserver(
          speech_recognition_availability_observer_.BindNewPipeAndPassRemote());

  render_frame_->GetBrowserInterfaceBroker()->GetInterface(
      std::move(speech_recognition_client_browser_interface_receiver));
}

ChromeSpeechRecognitionClient::~ChromeSpeechRecognitionClient() = default;

void ChromeSpeechRecognitionClient::AddAudio(
    scoped_refptr<media::AudioBuffer> buffer) {
  DCHECK(buffer);
  send_audio_callback_.Run(
      ConvertToAudioDataS16(std::move(buffer), is_multichannel_supported_));
}

void ChromeSpeechRecognitionClient::AddAudio(
    std::unique_ptr<media::AudioBus> audio_bus,
    int sample_rate,
    media::ChannelLayout channel_layout) {
  DCHECK(audio_bus);
  send_audio_callback_.Run(ConvertToAudioDataS16(std::move(audio_bus),
                                                 sample_rate, channel_layout,
                                                 is_multichannel_supported_));
}

bool ChromeSpeechRecognitionClient::IsSpeechRecognitionAvailable() {
  return !is_website_blocked_ && is_browser_requesting_transcription_ &&
         is_recognizer_bound_;
}

// The OnReadyCallback is set by the owner of |this| and is executed when speech
// recognition becomes available. Setting the callback will override any
// existing callback.
void ChromeSpeechRecognitionClient::SetOnReadyCallback(
    SpeechRecognitionClient::OnReadyCallback callback) {
  on_ready_callback_ = std::move(callback);

  // Immediately run the callback if speech recognition is already available.
  if (IsSpeechRecognitionAvailable() && on_ready_callback_)
    std::move(on_ready_callback_).Run();
}

void ChromeSpeechRecognitionClient::OnRecognizerBound(
    bool is_multichannel_supported) {
  is_multichannel_supported_ = is_multichannel_supported;
  is_recognizer_bound_ = true;

  if (on_ready_callback_)
    std::move(on_ready_callback_).Run();
}

void ChromeSpeechRecognitionClient::OnSpeechRecognitionRecognitionEvent(
    media::mojom::SpeechRecognitionResultPtr result) {
  if (!caption_host_.is_bound())
    return;
  caption_host_->OnTranscription(
      chrome::mojom::TranscriptionResult::New(result->transcription,
                                              result->is_final),
      base::BindOnce(&ChromeSpeechRecognitionClient::OnTranscriptionCallback,
                     base::Unretained(this)));
}

void ChromeSpeechRecognitionClient::OnSpeechRecognitionError() {
  if (caption_host_.is_bound())
    caption_host_->OnError();
}

void ChromeSpeechRecognitionClient::OnLanguageIdentificationEvent(
    media::mojom::LanguageIdentificationEventPtr event) {
  caption_host_->OnLanguageIdentificationEvent(std::move(event));
}

void ChromeSpeechRecognitionClient::SpeechRecognitionAvailabilityChanged(
    bool is_speech_recognition_available) {
  if (is_speech_recognition_available) {
    initialize_callback_.Run();
  } else if (reset_callback_) {
    reset_callback_.Run();
  }
}

void ChromeSpeechRecognitionClient::SpeechRecognitionLanguageChanged(
    const std::string& language) {
  speech_recognition_recognizer_->OnLanguageChanged(language);
}

void ChromeSpeechRecognitionClient::Initialize() {
  if (speech_recognition_context_.is_bound())
    return;

  mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_receiver =
          speech_recognition_context_.BindNewPipeAndPassReceiver();
  speech_recognition_context_->BindRecognizer(
      speech_recognition_recognizer_.BindNewPipeAndPassReceiver(),
      speech_recognition_client_receiver_.BindNewPipeAndPassRemote(),
      media::BindToCurrentLoop(
          base::BindOnce(&ChromeSpeechRecognitionClient::OnRecognizerBound,
                         weak_factory_.GetWeakPtr())));

  render_frame_->GetBrowserInterfaceBroker()->GetInterface(
      std::move(speech_recognition_context_receiver));
  render_frame_->GetBrowserInterfaceBroker()->GetInterface(
      caption_host_.BindNewPipeAndPassReceiver());

  if (base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption)) {
    is_website_blocked_ = false;
  } else {
    is_website_blocked_ = IsUrlBlocked(
        render_frame_->GetWebFrame()->GetSecurityOrigin().ToString().Utf8());
    base::UmaHistogramBoolean("Accessibility.LiveCaption.WebsiteBlocked",
                              is_website_blocked_);
  }

  // Bind the call to Reset() to the Media thread.
  reset_callback_ = media::BindToCurrentLoop(base::BindRepeating(
      &ChromeSpeechRecognitionClient::Reset, weak_factory_.GetWeakPtr()));

  speech_recognition_context_.set_disconnect_handler(media::BindToCurrentLoop(
      base::BindOnce(&ChromeSpeechRecognitionClient::OnRecognizerDisconnected,
                     weak_factory_.GetWeakPtr())));

  // Unretained is safe because |this| owns the mojo::Remote.
  caption_host_.set_disconnect_handler(
      base::BindOnce(&ChromeSpeechRecognitionClient::OnCaptionHostDisconnected,
                     base::Unretained(this)));
}

void ChromeSpeechRecognitionClient::Reset() {
  is_recognizer_bound_ = false;
  is_browser_requesting_transcription_ = true;
  speech_recognition_context_.reset();
  speech_recognition_recognizer_.reset();
  speech_recognition_client_receiver_.reset();
  caption_host_.reset();
}

void ChromeSpeechRecognitionClient::SendAudioToSpeechRecognitionService(
    media::mojom::AudioDataS16Ptr audio_data) {
  DCHECK(audio_data);
  if (!speech_recognition_recognizer_.is_bound())
    return;
  if (IsSpeechRecognitionAvailable()) {
    speech_recognition_recognizer_->SendAudioToSpeechRecognitionService(
        std::move(audio_data));
  } else if (is_recognizer_bound_) {
    speech_recognition_recognizer_->AudioReceivedAfterBubbleClosed(
        media::AudioTimestampHelper::FramesToTime(audio_data->frame_count,
                                                  audio_data->sample_rate));
  }
}

void ChromeSpeechRecognitionClient::OnTranscriptionCallback(bool success) {
  if (!success && is_browser_requesting_transcription_ &&
      speech_recognition_recognizer_.is_bound()) {
    speech_recognition_recognizer_->OnCaptionBubbleClosed();
  }

  is_browser_requesting_transcription_ = success;
}

bool ChromeSpeechRecognitionClient::IsUrlBlocked(const std::string& url) const {
  return blocked_urls_.find(url) != blocked_urls_.end();
}

void ChromeSpeechRecognitionClient::OnRecognizerDisconnected() {
  is_recognizer_bound_ = false;
  if (caption_host_.is_bound())
    caption_host_->OnError();
}

void ChromeSpeechRecognitionClient::OnCaptionHostDisconnected() {
  is_browser_requesting_transcription_ = false;
}
