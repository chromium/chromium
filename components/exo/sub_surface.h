// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SUB_SURFACE_H_
#define COMPONENTS_EXO_SUB_SURFACE_H_

#include <memory>

#include "base/macros.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/surface_observer.h"
#include "ui/gfx/geometry/point.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace exo {
class Surface;

// This class provides functions for treating a surface as a sub-surface. A
// sub-surface has one parent surface.
class SubSurface : public SurfaceDelegate, public SurfaceObserver {
 public:
  SubSurface(Surface* surface, Surface* parent);
  ~SubSurface() override;

  // This schedules a sub-surface position change. The sub-surface will be
  // moved so, that its origin (top-left corner pixel) will be at the |position|
  // of the parent surface coordinate system.
  void SetPosition(const gfx::Point& position);

  // This removes sub-surface from the stack, and puts it back just above the
  // reference surface, changing the z-order of the sub-surfaces. The reference
  // surface must be one of the sibling surfaces, or the parent surface.
  void PlaceAbove(Surface* reference);

  // This removes sub-surface from the stack, and puts it back just below the
  // sibling surface.
  void PlaceBelow(Surface* sibiling);

  // Change the commit behaviour of the sub-surface.
  void SetCommitBehavior(bool synchronized);

  // Returns a trace value representing the state of the surface.
  std::unique_ptr<base::trace_event::TracedValue> AsTracedValue() const;

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;
  bool IsSurfaceSynchronized() const override;
  bool IsInputEnabled(Surface* surface) const override;
  void OnSetFrame(SurfaceFrameType type) override {}
  void OnSetFrameColors(SkColor active_color, SkColor inactive_color) override {
  }
  void OnSetParent(Surface* parent, const gfx::Point& position) override;
  void OnSetStartupId(const char* startup_id) override {}
  void OnSetApplicationId(const char* application_id) override {}
  void SetUseImmersiveForFullscreen(bool value) override {}
  void OnActivationRequested() override {}
  void OnNewOutputAdded() override {}
  void OnSetServerStartResize() override {}
  void ShowSnapPreviewToLeft() override {}
  void ShowSnapPreviewToRight() override {}
  void HideSnapPreview() override {}
  void SetSnappedToLeft() override {}
  void SetSnappedToRight() override {}
  void UnsetSnap() override {}
  void SetCanGoBack() override {}
  void UnsetCanGoBack() override {}

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

 private:
  Surface* surface_;
  Surface* parent_;
  bool is_synchronized_ = true;

  DISALLOW_COPY_AND_ASSIGN(SubSurface);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SUB_SURFACE_H_
