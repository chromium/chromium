// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_CONTEXT_REMOTE_H_
#define COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_CONTEXT_REMOTE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/live_caption/caption_bubble_context.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/rect.h"

namespace captions {

class CaptionBubbleSessionObserver;
class CaptionBubbleSessionObserverRemote;

// The bubble context for captions generated in a remote process. Methods of
// this class delegate to the remote implementation via Mojo.
class CaptionBubbleContextRemote : public CaptionBubbleContext {
 public:
  CaptionBubbleContextRemote(
      mojo::PendingRemote<media::mojom::SpeechRecognitionSurface> surface,
      const std::string& session_id);
  ~CaptionBubbleContextRemote() override;
  CaptionBubbleContextRemote(const CaptionBubbleContextRemote&) = delete;
  CaptionBubbleContextRemote& operator=(const CaptionBubbleContextRemote&) =
      delete;

  // CaptionBubbleContext:
  void GetBounds(GetBoundsCallback callback) const override;
  const std::string GetSessionId() const override;
  void Activate() override;
  bool IsActivatable() const override;
  std::unique_ptr<CaptionBubbleSessionObserver>
  GetCaptionBubbleSessionObserver() override;
  OpenCaptionSettingsCallback GetOpenCaptionSettingsCallback() override;

  // Triggers the end-of-session callback if there is an active caption bubble
  // session observer.
  void OnSessionEnded();

 private:
  const std::string session_id_;

  base::WeakPtr<CaptionBubbleSessionObserverRemote> session_observer_;

  mojo::Remote<media::mojom::SpeechRecognitionSurface> surface_;
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_CONTEXT_REMOTE_H_
