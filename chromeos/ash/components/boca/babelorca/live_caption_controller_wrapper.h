// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_LIVE_CAPTION_CONTROLLER_WRAPPER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_LIVE_CAPTION_CONTROLLER_WRAPPER_H_

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

namespace ash::babelorca {

// An interface to wrap needed functions of live caption controller.
class LiveCaptionControllerWrapper {
 public:
  LiveCaptionControllerWrapper(const LiveCaptionControllerWrapper&) = delete;
  LiveCaptionControllerWrapper& operator=(const LiveCaptionControllerWrapper&) =
      delete;

  virtual ~LiveCaptionControllerWrapper() = default;

  virtual bool DispatchTranscription(
      const media::SpeechRecognitionResult& result) = 0;

  virtual void ToggleLiveCaptionForBabelOrca(bool enabled) = 0;

  virtual void OnAudioStreamEnd() = 0;

  virtual void RestartCaptions() = 0;

 protected:
  LiveCaptionControllerWrapper() = default;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_LIVE_CAPTION_CONTROLLER_WRAPPER_H_
