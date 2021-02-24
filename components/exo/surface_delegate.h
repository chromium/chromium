// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SURFACE_DELEGATE_H_
#define COMPONENTS_EXO_SURFACE_DELEGATE_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"

namespace exo {
class Surface;

// Frame types that can be used to decorate a surface.
enum class SurfaceFrameType { NONE, NORMAL, SHADOW, AUTOHIDE, OVERLAY };

// Handles events on surfaces in context-specific ways.
class SurfaceDelegate {
 public:
  // Called when surface was requested to commit all double-buffered state.
  virtual void OnSurfaceCommit() = 0;

  // Returns true if surface is in synchronized mode. ie. commit of
  // double-buffered state should be synchronized with parent surface.
  virtual bool IsSurfaceSynchronized() const = 0;

  // Returns true if surface should receive input events.
  virtual bool IsInputEnabled(Surface* surface) const = 0;

  // Called when surface was requested to use a specific frame type.
  virtual void OnSetFrame(SurfaceFrameType type) = 0;

  // Called when surface was requested to use a specific set of frame colors.
  virtual void OnSetFrameColors(SkColor active_color,
                                SkColor inactive_color) = 0;

  // Called when a new "parent" was requested for this surface. |position|
  // is the initial position of surface relative to origin of parent.
  virtual void OnSetParent(Surface* parent, const gfx::Point& position) = 0;

  // Called when surface was requested to set a specific startup ID label.
  virtual void OnSetStartupId(const char* startup_id) = 0;

  // Called when surface was requested to set a specific application ID label.
  virtual void OnSetApplicationId(const char* application_id) = 0;

  // Whether to hide the shelf when fullscreen. If true, shelf is inaccessible
  // (plain fullscreen). If false, shelf auto-hides and can be shown with a
  // mouse gesture (immersive fullscreen).
  virtual void SetUseImmersiveForFullscreen(bool value) = 0;

  // Called when the surface's application wants it to be activated.
  virtual void OnActivationRequested() = 0;

  // Called when the new outoupt resource is created.
  virtual void OnNewOutputAdded() = 0;

  // Called when surface was requested to start resize.
  virtual void OnSetServerStartResize() = 0;

  // Called to show the snap preview to the right or left, or to hide it.
  virtual void ShowSnapPreviewToLeft() = 0;
  virtual void ShowSnapPreviewToRight() = 0;
  virtual void HideSnapPreview() = 0;

  // Called when the client was snapped to right or left, and reset.
  virtual void SetSnappedToLeft() = 0;
  virtual void SetSnappedToRight() = 0;
  virtual void UnsetSnap() = 0;

 protected:
  virtual ~SurfaceDelegate() {}
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SURFACE_DELEGATE_H_
