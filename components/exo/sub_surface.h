// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SUB_SURFACE_H_
#define COMPONENTS_EXO_SUB_SURFACE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/surface_observer.h"
#include "ui/base/class_property.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"

namespace base {
namespace trace_event {
class TracedValue;
}
}  // namespace base

namespace exo {
class Surface;
class SubSurfaceObserver;

// This class provides functions for treating a surface as a sub-surface. A
// sub-surface has one parent surface.
class SubSurface : public SurfaceDelegate,
                   public SurfaceObserver,
                   public ui::PropertyHandler {
 public:
  SubSurface(Surface* surface, Surface* parent);

  SubSurface(const SubSurface&) = delete;
  SubSurface& operator=(const SubSurface&) = delete;

  ~SubSurface() override;

  Surface* surface() { return surface_; }

  // This schedules a sub-surface position change. The sub-surface will be
  // moved so, that its origin (top-left corner pixel) will be at the |position|
  // of the parent surface coordinate system.
  void SetPosition(const gfx::PointF& position);

  // This schedules a matrix transform to be applied when drawing this
  // sub-surface.
  void SetTransform(const gfx::Transform& transform);

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
  void ShowSnapPreviewToPrimary() override {}
  void ShowSnapPreviewToSecondary() override {}
  void HideSnapPreview() override {}
  void SetSnapPrimary(float snap_ratio) override {}
  void SetSnapSecondary(float snap_ratio) override {}
  void UnsetSnap() override {}
  void SetCanGoBack() override {}
  void UnsetCanGoBack() override {}
  void SetPip() override {}
  void UnsetPip() override {}
  void SetFloatToLocation(
      chromeos::FloatStartLocation float_start_location) override {}
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override {}
  void MoveToDesk(int desk_index) override {}
  void SetVisibleOnAllWorkspaces() override {}
  void SetInitialWorkspace(const char* initial_workspace) override {}
  void Pin(bool trusted) override {}
  void Unpin() override {}
  void SetSystemModal(bool system_modal) override {}
  void SetTopInset(int height) override {}
  SecurityDelegate* GetSecurityDelegate() override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  // SubSurface Observers
  void AddSubSurfaceObserver(SubSurfaceObserver* observer);
  void RemoveSubSurfaceObserver(SubSurfaceObserver* observer);

 private:
  raw_ptr<Surface> surface_;
  raw_ptr<Surface> parent_;
  bool is_synchronized_ = true;

  // Surface observer list. Surface does not own the observers.
  base::ObserverList<SubSurfaceObserver> observers_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SUB_SURFACE_H_
