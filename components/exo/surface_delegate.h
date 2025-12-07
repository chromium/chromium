// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SURFACE_DELEGATE_H_
#define COMPONENTS_EXO_SURFACE_DELEGATE_H_

#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size_f.h"

namespace exo {
class SecurityDelegate;
class Surface;

// Frame types that can be used to decorate a surface.
enum class SurfaceFrameType {
  NONE,
  NORMAL,
  SHADOW,
  AUTOHIDE,
  OVERLAY,
  OVERLAP
};

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

  // Whether to show/hide the shelf when fullscreen. If true, the titlebar/shelf
  // will show when the mouse moves to the top/bottom of the screen. If false
  // (plain fullscreen), the titlebar and shelf are always hidden.
  virtual void SetUseImmersiveForFullscreen(bool value) = 0;

  // Called when the surface's application wants it to be activated.
  virtual void OnActivationRequested() = 0;

  // Called when the new outoupt resource is created.
  virtual void OnNewOutputAdded() = 0;

  // Called when surface was requested to start resize.
  virtual void OnSetServerStartResize() = 0;

  // Called to show the snap preview to the primary or secondary position, or
  // to hide it.
  virtual void ShowSnapPreviewToPrimary() = 0;
  virtual void ShowSnapPreviewToSecondary() = 0;
  virtual void HideSnapPreview() = 0;

  // Called when the client was snapped to primary or secondary position, and
  // reset.
  virtual void SetSnapPrimary(float snap_ratio) = 0;
  virtual void SetSnapSecondary(float snap_ratio) = 0;
  virtual void UnsetSnap() = 0;

  // Whether the current client window can go back, as per its navigation list.
  virtual void SetCanGoBack() = 0;
  virtual void UnsetCanGoBack() = 0;

  // Called when surface was requested to enter pip.
  virtual void SetPip() = 0;
  virtual void UnsetPip() = 0;

  // Floats the shell surface. The bounds of the surface are determined by
  // `float_start_location`.
  virtual void SetFloatToLocation(
      chromeos::FloatStartLocation float_start_location) = 0;

  // Called when surface was requested to maintain an aspect ratio.
  virtual void SetAspectRatio(const gfx::SizeF& aspect_ratio) = 0;

  // Called when surface was requested to move the window to a desk at
  // |desk_index|.
  virtual void MoveToDesk(int desk_index) = 0;

  // Called when surface was requested to be visible on all workspaces.
  virtual void SetVisibleOnAllWorkspaces() = 0;

  // Called to set the initial workspace to restore a window to the
  // corresponding desk.
  virtual void SetInitialWorkspace(const char* initial_workspace) = 0;

  // Pins/locks a window to the screen so that the user cannot do anything
  // else before the mode is released. If trusted is set, it is an invocation
  // from a trusted app like a school test mode app.
  virtual void Pin(bool trusted) = 0;

  // Releases the pinned mode and allows the user to do other things again.
  virtual void Unpin() = 0;

  // Sets the system modality.
  virtual void SetSystemModal(bool modal) = 0;

  // Sets the top inset (header height).
  virtual void SetTopInset(int height) = 0;

  // Returns the SecurityDelegate which this surface should use to perform
  // security-sensitive operations. See go/secure-exo-ids for more information.
  virtual SecurityDelegate* GetSecurityDelegate() = 0;

 protected:
  virtual ~SurfaceDelegate() = default;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SURFACE_DELEGATE_H_
