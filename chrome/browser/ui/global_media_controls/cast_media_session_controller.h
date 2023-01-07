// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_SESSION_CONTROLLER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_SESSION_CONTROLLER_H_

#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "components/media_router/common/mojom/media_controller.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace base {
class TimeDelta;
}  // namespace base

// Forwards media action commands to media_router::mojom::MediaController.
class CastMediaSessionController {
 public:
  CastMediaSessionController(
      mojo::Remote<media_router::mojom::MediaController> route_controller);
  CastMediaSessionController(const CastMediaSessionController&) = delete;
  CastMediaSessionController& operator=(const CastMediaSessionController&) =
      delete;
  virtual ~CastMediaSessionController();

  // Forwards |action| to the MediaController. No-ops if OnMediaStatusUpdated()
  // has not been called.
  virtual void Send(media_session::mojom::MediaSessionAction action);

  virtual void OnMediaStatusUpdated(
      media_router::mojom::MediaStatusPtr media_status);

  virtual void SeekTo(base::TimeDelta time);

  virtual void SetMute(bool mute);

  virtual void SetVolume(float volume);

  void FlushForTesting();
  media_router::mojom::MediaStatusPtr GetMediaStatusForTesting();

 private:
  base::TimeDelta PutWithinBounds(const base::TimeDelta& time);

  void IncrementCurrentTimeAfterOneSecond();
  void IncrementCurrentTime();

  mojo::Remote<media_router::mojom::MediaController> route_controller_;
  media_router::mojom::MediaStatusPtr media_status_;
  base::CancelableOnceClosure increment_current_time_callback_;
  base::WeakPtrFactory<CastMediaSessionController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_SESSION_CONTROLLER_H_
