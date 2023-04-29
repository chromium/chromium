// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LACROS_SNAP_CONTROLLER_LACROS_H_
#define CHROME_BROWSER_UI_LACROS_SNAP_CONTROLLER_LACROS_H_

#include "chromeos/ui/frame/caption_buttons/snap_controller.h"

// Stub Lacros implementation of chromeos::SnapController.
class SnapControllerLacros : public chromeos::SnapController {
 public:
  SnapControllerLacros();
  SnapControllerLacros(const SnapControllerLacros&) = delete;
  SnapControllerLacros& operator=(const SnapControllerLacros&) = delete;
  ~SnapControllerLacros() override;

  // chromeos::SnapController:
  bool CanSnap(aura::Window* window) override;
  void ShowSnapPreview(aura::Window* window,
                       chromeos::SnapDirection snap,
                       bool allow_haptic_feedback) override;
  void CommitSnap(aura::Window* window,
                  chromeos::SnapDirection snap,
                  float snap_ratio,
                  SnapRequestSource snap_request_source) override;
};

#endif  // CHROME_BROWSER_UI_LACROS_SNAP_CONTROLLER_LACROS_H_
