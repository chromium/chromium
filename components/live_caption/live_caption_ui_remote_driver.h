// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_LIVE_CAPTION_UI_REMOTE_DRIVER_H_
#define COMPONENTS_LIVE_CAPTION_LIVE_CAPTION_UI_REMOTE_DRIVER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/live_caption/caption_bubble_context_remote.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace captions {

class CaptionBubbleContextRemote;
class LiveCaptionController;

// Receives both speech recognition events and speech surface events (e.g.
// fullscreen-ing) from remote lacros processes, passing them to the live
// caption UI.
//
// One driver exists in the Ash browser process for each caption-producing
// stream of media in a lacros renderer.
class LiveCaptionUiRemoteDriver
    : public media::mojom::SpeechRecognitionSurfaceClient,
      public media::mojom::SpeechRecognitionRecognizerClient {
 public:
  // The speech surface client implementation is bound at construction time, but
  // the speech recognition client implementation is bound in the "client
  // browser interface"'s receiver set.
  LiveCaptionUiRemoteDriver(
      LiveCaptionController* controller,
      mojo::PendingReceiver<media::mojom::SpeechRecognitionSurfaceClient>
          client_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionSurface> surface,
      const std::string& session_id);
  ~LiveCaptionUiRemoteDriver() override;

  // media::mojom::SpeechOriginRecognizerClient:
  void OnSpeechRecognitionRecognitionEvent(
      const media::SpeechRecognitionResult& result,
      OnSpeechRecognitionRecognitionEventCallback reply) override;
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override;
  void OnSpeechRecognitionError() override;
  void OnSpeechRecognitionStopped() override;

  // media::mojom::SpeechRecognitionSurfaceClient:
  void OnSessionEnded() override;
  void OnFullscreenToggled() override;

 private:
  // We are owned by the "client browser interface", which is a service that
  // `DependsOn` the controller. Hence the controller is guaranteed to exist.
  const raw_ptr<LiveCaptionController> controller_;

  mojo::Receiver<media::mojom::SpeechRecognitionSurfaceClient> client_receiver_;

  CaptionBubbleContextRemote context_;
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_LIVE_CAPTION_UI_REMOTE_DRIVER_H_
