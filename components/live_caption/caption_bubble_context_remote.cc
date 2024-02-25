// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/caption_bubble_context_remote.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "components/live_caption/caption_bubble_session_observer.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace captions {

// Holds a callback to be run when a live captioning "session" (essentially,
// caption generation for one page) ends for a fixed caption bubble context. The
// context is responsible for signaling a session end when e.g. a navigation
// occurs or the page is reloaded.
//
// In the Ash browser, the caption bubble controller creates and holds one of
// these objects for each context. The object is then manually destroyed when
// the context is invalidated.
class CaptionBubbleSessionObserverRemote : public CaptionBubbleSessionObserver {
 public:
  explicit CaptionBubbleSessionObserverRemote(const std::string& session_id)
      : session_id_(session_id) {}
  ~CaptionBubbleSessionObserverRemote() override = default;
  CaptionBubbleSessionObserverRemote(
      const CaptionBubbleSessionObserverRemote&) = delete;
  CaptionBubbleSessionObserverRemote& operator=(
      const CaptionBubbleSessionObserverRemote&) = delete;

  void SetEndSessionCallback(EndSessionCallback callback) override {
    callback_ = std::move(callback);
  }

  void OnSessionEnded() {
    if (callback_) {
      std::move(callback_).Run(session_id_);
    }
  }

  base::WeakPtr<CaptionBubbleSessionObserverRemote> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const std::string session_id_;

  EndSessionCallback callback_;

  base::WeakPtrFactory<CaptionBubbleSessionObserverRemote> weak_ptr_factory_{
      this};
};

CaptionBubbleContextRemote::CaptionBubbleContextRemote(
    mojo::PendingRemote<media::mojom::SpeechRecognitionSurface> surface,
    const std::string& session_id)
    : session_id_(session_id), surface_(std::move(surface)) {}

CaptionBubbleContextRemote::~CaptionBubbleContextRemote() {
  if (session_observer_) {
    session_observer_->OnSessionEnded();
  }
}

void CaptionBubbleContextRemote::GetBounds(GetBoundsCallback callback) const {
  // Forwards the rect to our callback only if it is non-nullopt.
  auto maybe_run_cb = [](GetBoundsCallback cb,
                         const std::optional<gfx::Rect>& bounds) {
    if (bounds.has_value()) {
      std::move(cb).Run(*bounds);
    }
  };

  surface_->GetBounds(
      base::BindOnce(std::move(maybe_run_cb), std::move(callback)));
}

const std::string CaptionBubbleContextRemote::GetSessionId() const {
  return session_id_;
}

void CaptionBubbleContextRemote::Activate() {
  surface_->Activate();
}

bool CaptionBubbleContextRemote::IsActivatable() const {
  return true;
}

std::unique_ptr<CaptionBubbleSessionObserver>
CaptionBubbleContextRemote::GetCaptionBubbleSessionObserver() {
  auto session_observer =
      std::make_unique<CaptionBubbleSessionObserverRemote>(session_id_);
  session_observer_ = session_observer->GetWeakPtr();
  return session_observer;
}

void CaptionBubbleContextRemote::OnSessionEnded() {
  if (session_observer_) {
    session_observer_->OnSessionEnded();
  }
}

OpenCaptionSettingsCallback
CaptionBubbleContextRemote::GetOpenCaptionSettingsCallback() {
  NOTIMPLEMENTED();
  return base::RepeatingClosure();
}

}  // namespace captions
