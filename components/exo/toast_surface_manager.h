// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TOAST_SURFACE_MANAGER_H_
#define COMPONENTS_EXO_TOAST_SURFACE_MANAGER_H_

namespace exo {

class ToastSurface;

class ToastSurfaceManager {
 public:
  virtual ~ToastSurfaceManager() = default;

  // Adds an ToastSurface to the manager.
  virtual void AddSurface(ToastSurface* surface) = 0;

  // Removes an ToastSurface from the manager.
  virtual void RemoveSurface(ToastSurface* surface) = 0;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_TOAST_SURFACE_MANAGER_H_
