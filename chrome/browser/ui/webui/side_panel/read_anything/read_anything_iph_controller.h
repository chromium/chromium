// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_IPH_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_IPH_CONTROLLER_H_

#include <optional>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/read_anything/read_anything_lifecycle_observer.h"
#include "chrome/common/read_anything/read_anything.mojom-shared.h"

class ReadAnythingController;

// Shows and closes Reading Mode's IPH. Observes Reading Mode opening and
// closing, and is told by its owner, ReadAnythingUntrustedPageHandler, about
// internal Reading Mode state changes.
class ReadAnythingIphController : public ReadAnythingLifecycleObserver {
 public:
  explicit ReadAnythingIphController(
      ReadAnythingController* read_anything_controller);
  ReadAnythingIphController(const ReadAnythingIphController&) = delete;
  ReadAnythingIphController& operator=(const ReadAnythingIphController&) =
      delete;
  ~ReadAnythingIphController() override;

  // Overrides the delay before the AI playback promo is shown, until the
  // returned object is destroyed.
  static base::AutoReset<base::TimeDelta> SetAiPlaybackIphDelayForTesting(
      base::TimeDelta delay);

  void OnDistillationStateChanged(
      read_anything::mojom::ReadAnythingDistillationState new_state);
  void OnAiPlaybackStateChanged(bool enabled);
  void OnLineFocusTurnedOn();

  // ReadAnythingLifecycleObserver:
  void Activate(
      bool active,
      read_anything::mojom::ReadAnythingOpenTrigger trigger,
      std::optional<base::TimeDelta> completed_session_duration) override;
  void OnDestroyed() override;

 private:
  void MaybeStartAiPlaybackIphTimer();
  void MaybeShowAiPlaybackIph();

  // Null once ReadAnythingController is destroyed.
  raw_ptr<ReadAnythingController> read_anything_controller_;
  // The last distillation state reported.
  read_anything::mojom::ReadAnythingDistillationState distillation_state_ =
      read_anything::mojom::ReadAnythingDistillationState::kUndefined;
  // Shows the AI playback promo once Reading Mode has shown content for a
  // while.
  base::OneShotTimer ai_playback_iph_timer_;
  base::ScopedObservation<ReadAnythingController, ReadAnythingLifecycleObserver>
      read_anything_controller_observer_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_IPH_CONTROLLER_H_
