// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_INPUT_METHOD_SURFACE_MANAGER_H_
#define COMPONENTS_EXO_INPUT_METHOD_SURFACE_MANAGER_H_

namespace exo {

class InputMethodSurface;

class InputMethodSurfaceManager {
 public:
  virtual ~InputMethodSurfaceManager() = default;

  // Gets the InputMethodSurface currently used.
  virtual InputMethodSurface* GetSurface() const = 0;

  // Adds an InputMethodSurface to the manager.
  virtual void AddSurface(InputMethodSurface* surface) = 0;

  // Removes an InputMethodSurface from the manager.
  virtual void RemoveSurface(InputMethodSurface* surface) = 0;

  // Called when a touchable bounds of an InputMethodSurface changed.
  virtual void OnTouchableBoundsChanged(InputMethodSurface* surface) = 0;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_INPUT_METHOD_SURFACE_MANAGER_H_
