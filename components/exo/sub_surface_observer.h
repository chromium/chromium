// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SUB_SURFACE_OBSERVER_H_
#define COMPONENTS_EXO_SUB_SURFACE_OBSERVER_H_

#include "base/observer_list_types.h"

namespace exo {
class SubSurface;

// Observers can listen to various events on the SubSurfaces.
class SubSurfaceObserver : public base::CheckedObserver {
 public:
  // Called at the top of the sub-surface's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnSubSurfaceDestroying(SubSurface* sub_surface) = 0;

 protected:
  ~SubSurfaceObserver() override = default;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SUB_SURFACE_OBSERVER_H_
