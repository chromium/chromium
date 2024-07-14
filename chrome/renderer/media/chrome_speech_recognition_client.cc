// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/chrome_speech_recognition_client.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "media/audio/reconfigurable_audio_bus_pool.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_mixer.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"

// Preallocate 500ms worth of buffers when using a ReconfigurableAudioBusPool.
constexpr base::TimeDelta kAudioBusPoolDuration = base::Milliseconds(500);

ChromeSpeechRecognitionClient::ChromeSpeechRecognitionClient(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  initialize_callback_ = base::BindPostTaskToCurrentDefault(base::BindRepeating(
      &ChromeSpeechRecognitionClient::Initialize, weak_factory_.GetWeakPtr()));

  send_audio_callback_ = base::BindPostTaskToCurrentDefault(base::BindRepeating(
      &ChromeSpeechRecognitionClient::SendAudioToSpeechRecognitionService,
      weak_factory_.GetWeakPtr()));

  mojo::PendingReceiver<media::mojom::SpeechRecognitionClientBrowserInterface>
      speech_recognition_client_browser_interface_receiver =
          speech_recognition_client_browser_interface_
              .BindNewPipeAndPassReceiver();
  speech_recognition_client_browser_interface_
      ->BindSpeechRecognitionBrowserObserver(
          speech_recognition_availability_observer_.BindNewPipeAndPassRemote());

  render_frame->GetBrowserInterfaceBroker().GetInterface(
      std::move(speech_recognition_client_browser_interface_receiver));

  add_audio_on_main_sequence_callback_ =
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &ChromeSpeechRecognitionClient::AddAudioBusOnMainSequence,
          weak_factory_.GetWeakPtr()));
  audio_bus_pool_ = std::make_unique<media::ReconfigurableAudioBusPoolImpl>(
      kAudioBusPoolDuration);
}

ChromeSpeechRecognitionClient::~ChromeSpeechRecognitionClient() = default;

void ChromeSpeechRecognitionClient::AddAudio(
    scoped_refptr<media::AudioBuffer> buffer) {
  DCHECK(buffer);
  send_audio_callback_.Run(
      ConvertToAudioDataS16(std::move(buffer), is_multichannel_supported_));
}

void ChromeSpeechRecognitionClient::AddAudio(const media::AudioBus& audio_bus) {
  // IsSpeechRecognitionAvailable() will return false if there is contention on
  // is_recognizer_bound_lock_.
  if (!IsSpeechRecognitionAvailable()) {
    return;
  }

  auto audio_bus_copy = audio_bus_pool_->GetAudioBus();
  CHECK_EQ(audio_bus.channels(), audio_parameters_.channels());
  CHECK_EQ(audio_bus.frames(), audio_parameters_.frames_per_buffer());
  CHECK_EQ(audio_bus_copy->channels(), audio_parameters_.channels());
  CHECK_EQ(audio_bus_copy->frames(), audio_parameters_.frames_per_buffer());
  audio_bus.CopyTo(audio_bus_copy.get());

  // Since Reconfigure() is not allowed to concurrently run with AddAudio(),
  // it's safe to access |audio_parameters_| here. We pass the parameters into
  // the callback rather than accessing them in the callback itself on the
  // main thread because there may be Reconfigure() pending on the main
  // thread which would modify |audio_parameters_| before the callback is
  // executed.
  add_audio_on_main_sequence_callback_.Run(std::move(audio_bus_copy),
                                           audio_parameters_.sample_rate(),
                                           audio_parameters_.channel_layout());
}

bool ChromeSpeechRecognitionClient::IsSpeechRecognitionAvailable() {
  base::AutoTryLock try_locker(is_recognizer_bound_lock_);
  if (try_locker.is_acquired()) {
    return is_recognizer_bound_;
  }

  return false;
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

void ChromeSpeechRecognitionClient::Reconfigure(
    const media::AudioParameters& audio_parameters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  audio_bus_pool_->Reconfigure(audio_parameters);
  audio_parameters_ = audio_parameters;
}

void ChromeSpeechRecognitionClient::OnRecognizerBound(
    bool is_multichannel_supported) {
  {
    base::AutoLock auto_lock(is_recognizer_bound_lock_);
    is_recognizer_bound_ = true;
  }

  is_multichannel_supported_ = is_multichannel_supported;

  if (on_ready_callback_)
    std::move(on_ready_callback_).Run();
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
  if (speech_recognition_recognizer_.is_bound()) {
    speech_recognition_recognizer_->OnLanguageChanged(language);
  }
}

void ChromeSpeechRecognitionClient::SpeechRecognitionMaskOffensiveWordsChanged(
    bool mask_offensive_words) {
  if (speech_recognition_recognizer_.is_bound()) {
    speech_recognition_recognizer_->OnMaskOffensiveWordsChanged(
        mask_offensive_words);
  }
}

void ChromeSpeechRecognitionClient::OnDestruct() {
  // Do nothing. The lifetime of the ChromeSpeechRecognitionClient is managed by
  // the owner of the object. However, the ChromeSpeechRecognitionClient will
  // not be able to be initialized after the RenderFrame is destroyed.
}

void ChromeSpeechRecognitionClient::Initialize() {
  if (speech_recognition_context_.is_bound() || !render_frame())
    return;

  // Create a SpeechRecognitionRecognizerClient remote and bind it to the
  // render frame. The receiver is in the browser.
  mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
      speech_recognition_client_remote;
  render_frame()->GetBrowserInterfaceBroker().GetInterface(
      speech_recognition_client_remote.InitWithNewPipeAndPassReceiver());

  // Create a SpeechRecognitionContext and bind it to the render frame. The
  // SpeechRecognitionContext passes the SpeechRecognitionRecognizer receiver
  // and moves the SpeechRecognitionRecognizerClient. The receiver is in the
  // utility process on Linux/Mac/Windows and in the Ash process on ChromeOS.
  mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_receiver =
          speech_recognition_context_.BindNewPipeAndPassReceiver();
  media::mojom::SpeechRecognitionOptionsPtr options =
      media::mojom::SpeechRecognitionOptions::New();
  options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
  options->enable_formatting = true;
  options->recognizer_client_type =
      media::mojom::RecognizerClientType::kLiveCaption;
  options->skip_continuously_empty_audio = true;

  speech_recognition_context_->BindRecognizer(
      speech_recognition_recognizer_.BindNewPipeAndPassReceiver(),
      std::move(speech_recognition_client_remote), std::move(options),
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&ChromeSpeechRecognitionClient::OnRecognizerBound,
                         weak_factory_.GetWeakPtr())));
  render_frame()->GetBrowserInterfaceBroker().GetInterface(
      std::move(speech_recognition_context_receiver));

  // Bind the call to Reset() to the Media thread.
  reset_callback_ = base::BindPostTaskToCurrentDefault(base::BindRepeating(
      &ChromeSpeechRecognitionClient::Reset, weak_factory_.GetWeakPtr()));

  speech_recognition_context_.set_disconnect_handler(
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &ChromeSpeechRecognitionClient::OnRecognizerDisconnected,
          weak_factory_.GetWeakPtr())));
}

void ChromeSpeechRecognitionClient::Reset() {
  base::AutoLock auto_lock(is_recognizer_bound_lock_);
  is_recognizer_bound_ = false;
  speech_recognition_context_.reset();
  speech_recognition_recognizer_.reset();
}

void ChromeSpeechRecognitionClient::AddAudioBusOnMainSequence(
    std::unique_ptr<media::AudioBus> audio_bus,
    int sample_rate,
    media::ChannelLayout channel_layout) {
  DCHECK(audio_bus);
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  send_audio_callback_.Run(ConvertToAudioDataS16(*audio_bus.get(), sample_rate,
                                                 channel_layout,
                                                 is_multichannel_supported_));

  if (audio_bus_pool_) {
    audio_bus_pool_->InsertAudioBus(std::move(audio_bus));
  }
}
void ChromeSpeechRecognitionClient::SendAudioToSpeechRecognitionService(
    media::mojom::AudioDataS16Ptr audio_data) {
  DCHECK(audio_data);
  if (speech_recognition_recognizer_.is_bound() &&
      IsSpeechRecognitionAvailable()) {
    speech_recognition_recognizer_->SendAudioToSpeechRecognitionService(
        std::move(audio_data));
  }
}

void ChromeSpeechRecognitionClient::OnRecognizerDisconnected() {
  base::AutoLock auto_lock(is_recognizer_bound_lock_);
  is_recognizer_bound_ = false;
}
