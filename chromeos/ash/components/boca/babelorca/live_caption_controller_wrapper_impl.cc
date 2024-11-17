// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/live_caption_controller_wrapper_impl.h"

#include <memory>
#include <utility>

#include "components/live_caption/caption_bubble_context.h"
#include "components/live_caption/live_caption_controller.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace ash::babelorca {

LiveCaptionControllerWrapperImpl::LiveCaptionControllerWrapperImpl(
    captions::LiveCaptionController* live_caption_controller,
    std::unique_ptr<captions::CaptionBubbleContext> caption_bubble_context)
    : live_caption_controller_(live_caption_controller),
      caption_bubble_context_(std::move(caption_bubble_context)) {}

LiveCaptionControllerWrapperImpl::~LiveCaptionControllerWrapperImpl() = default;

bool LiveCaptionControllerWrapperImpl::DispatchTranscription(
    const media::SpeechRecognitionResult& result) {
  return live_caption_controller_->DispatchTranscription(
      caption_bubble_context_.get(), result);
}

void LiveCaptionControllerWrapperImpl::ToggleLiveCaptionForBabelOrca(
    bool enabled) {
  live_caption_controller_->ToggleLiveCaptionForBabelOrca(enabled);
}

void LiveCaptionControllerWrapperImpl::OnAudioStreamEnd() {
  live_caption_controller_->OnAudioStreamEnd(caption_bubble_context_.get());
}

void LiveCaptionControllerWrapperImpl::RestartCaptions() {
  live_caption_controller_->ToggleLiveCaptionForBabelOrca(/*enabled=*/false);
  live_caption_controller_->ToggleLiveCaptionForBabelOrca(/*enabled=*/true);
}

}  // namespace ash::babelorca
