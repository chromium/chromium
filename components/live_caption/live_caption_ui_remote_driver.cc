// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/live_caption_ui_remote_driver.h"

#include <string>
#include <utility>

#include "base/functional/callback_forward.h"
#include "components/live_caption/caption_bubble_context_remote.h"
#include "components/live_caption/live_caption_controller.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace captions {

LiveCaptionUiRemoteDriver::LiveCaptionUiRemoteDriver(
    LiveCaptionController* controller,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionSurfaceClient>
        client_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionSurface> surface_remote,
    const std::string& session_id)
    : controller_(controller),
      client_receiver_(this, std::move(client_receiver)),
      context_(std::move(surface_remote), session_id) {}

LiveCaptionUiRemoteDriver::~LiveCaptionUiRemoteDriver() {
  controller_->OnAudioStreamEnd(&context_);
}

void LiveCaptionUiRemoteDriver::OnSpeechRecognitionRecognitionEvent(
    const media::SpeechRecognitionResult& result,
    OnSpeechRecognitionRecognitionEventCallback reply) {
  std::move(reply).Run(controller_->DispatchTranscription(&context_, result));
}

void LiveCaptionUiRemoteDriver::OnLanguageIdentificationEvent(
    media::mojom::LanguageIdentificationEventPtr event) {
  controller_->OnLanguageIdentificationEvent(&context_, std::move(event));
}

void LiveCaptionUiRemoteDriver::OnSpeechRecognitionError() {
  controller_->OnError(&context_, CaptionBubbleErrorType::kGeneric,
                       base::RepeatingClosure(),
                       base::BindRepeating([](CaptionBubbleErrorType error_type,
                                              bool checked) {}));
}

void LiveCaptionUiRemoteDriver::OnSpeechRecognitionStopped() {
  controller_->OnAudioStreamEnd(&context_);
}

void LiveCaptionUiRemoteDriver::OnSessionEnded() {
  context_.OnSessionEnded();
}

void LiveCaptionUiRemoteDriver::OnFullscreenToggled() {
  controller_->OnToggleFullscreen(&context_);
}

}  // namespace captions
