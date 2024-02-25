// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_INPUT_METHOD_SURFACE_H_
#define COMPONENTS_EXO_INPUT_METHOD_SURFACE_H_

#include "base/memory/raw_ptr.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/surface_observer.h"

namespace exo {

class InputMethodSurfaceManager;

// Handles input method surface role of a given surface.
class InputMethodSurface : public ClientControlledShellSurface {
 public:
  InputMethodSurface(InputMethodSurfaceManager* manager,
                     Surface* surface,
                     bool default_scale_cancellation);

  InputMethodSurface(const InputMethodSurface&) = delete;
  InputMethodSurface& operator=(const InputMethodSurface&) = delete;

  ~InputMethodSurface() override;

  static exo::InputMethodSurface* GetInputMethodSurface();

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;

  // Overridden from ShellSurfaceBase:
  void SetWidgetBounds(const gfx::Rect& bounds,
                       bool adjusted_by_server) override;

  gfx::Rect GetBounds() const;

 private:
  const raw_ptr<InputMethodSurfaceManager> manager_;
  bool added_to_manager_ = false;
  // The bounds of this surface in DIP.
  gfx::Rect input_method_bounds_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_INPUT_METHOD_SURFACE_H_
