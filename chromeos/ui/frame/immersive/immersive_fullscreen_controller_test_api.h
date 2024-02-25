// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_IMMERSIVE_IMMERSIVE_FULLSCREEN_CONTROLLER_TEST_API_H_
#define CHROMEOS_UI_FRAME_IMMERSIVE_IMMERSIVE_FULLSCREEN_CONTROLLER_TEST_API_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"

namespace chromeos {

class ImmersiveFullscreenController;

// Use by tests to access private state of ImmersiveFullscreenController.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) ImmersiveFullscreenControllerTestApi {
 public:
  explicit ImmersiveFullscreenControllerTestApi(
      ImmersiveFullscreenController* controller);

  ImmersiveFullscreenControllerTestApi(
      const ImmersiveFullscreenControllerTestApi&) = delete;
  ImmersiveFullscreenControllerTestApi& operator=(
      const ImmersiveFullscreenControllerTestApi&) = delete;

  ~ImmersiveFullscreenControllerTestApi();

  // Disables animations for any ImmersiveFullscreenControllers created while
  // GlobalAnimationDisabler exists.
  class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) GlobalAnimationDisabler {
   public:
    GlobalAnimationDisabler();

    GlobalAnimationDisabler(const GlobalAnimationDisabler&) = delete;
    GlobalAnimationDisabler& operator=(const GlobalAnimationDisabler&) = delete;

    ~GlobalAnimationDisabler();
  };

  // Disables animations and moves the mouse so that it is not over the
  // top-of-window views for the sake of testing.
  void SetupForTest();

  bool IsTopEdgeHoverTimerRunning() const;

  void EndAnimation();

 private:
  raw_ptr<ImmersiveFullscreenController, DanglingUntriaged>
      immersive_fullscreen_controller_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_IMMERSIVE_IMMERSIVE_FULLSCREEN_CONTROLLER_TEST_API_H_
