// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_LIVE_CAPTION_CONTROLLER_WRAPPER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_LIVE_CAPTION_CONTROLLER_WRAPPER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/boca/babelorca/live_caption_controller_wrapper.h"

namespace captions {
class CaptionBubbleContext;
class LiveCaptionController;
}  // namespace captions

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

namespace ash::babelorca {

// Implementation of live caption controller wrapper.
class LiveCaptionControllerWrapperImpl : public LiveCaptionControllerWrapper {
 public:
  LiveCaptionControllerWrapperImpl(
      captions::LiveCaptionController* live_caption_controller,
      std::unique_ptr<captions::CaptionBubbleContext> caption_bubble_context);

  ~LiveCaptionControllerWrapperImpl() override;

  bool DispatchTranscription(
      const media::SpeechRecognitionResult& result) override;

  void ToggleLiveCaptionForBabelOrca(bool enabled) override;

  void OnAudioStreamEnd() override;

  void RestartCaptions() override;

 private:
  raw_ptr<captions::LiveCaptionController> live_caption_controller_;
  std::unique_ptr<captions::CaptionBubbleContext> caption_bubble_context_;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_LIVE_CAPTION_CONTROLLER_WRAPPER_IMPL_H_
