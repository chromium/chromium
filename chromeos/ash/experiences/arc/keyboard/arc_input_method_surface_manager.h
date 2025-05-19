// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_KEYBOARD_ARC_INPUT_METHOD_SURFACE_MANAGER_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_KEYBOARD_ARC_INPUT_METHOD_SURFACE_MANAGER_H_

#include "ash/public/cpp/keyboard/arc/arc_input_method_bounds_tracker.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/experiences/arc/arc_export.h"
#include "components/exo/input_method_surface_manager.h"

namespace ash {

class ARC_EXPORT ArcInputMethodSurfaceManager
    : public exo::InputMethodSurfaceManager,
      public ArcInputMethodBoundsTracker {
 public:
  ArcInputMethodSurfaceManager();

  ArcInputMethodSurfaceManager(const ArcInputMethodSurfaceManager&) = delete;
  ArcInputMethodSurfaceManager& operator=(const ArcInputMethodSurfaceManager&) =
      delete;

  ~ArcInputMethodSurfaceManager() override;

  // exo::InputMethodSurfaceManager:
  exo::InputMethodSurface* GetSurface() const override;
  void AddSurface(exo::InputMethodSurface* surface) override;
  void RemoveSurface(exo::InputMethodSurface* surface) override;
  void OnTouchableBoundsChanged(exo::InputMethodSurface* surface) override;

 private:
  raw_ptr<exo::InputMethodSurface> input_method_surface_ =
      nullptr;  // Not owned
};

}  // namespace ash

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_KEYBOARD_ARC_INPUT_METHOD_SURFACE_MANAGER_H_
