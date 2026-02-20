// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TAB_DRAG_TARGET_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TAB_DRAG_TARGET_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/point.h"

struct DragSessionData;
class TabDragContext;

namespace tabs {
class TabModel;
}

// `TabDragTarget` is an interface that may be implemented to facilitate
// custom behavior beyond the tabstrip.
// TODO(crbug.com/394370034): The tabstrip currently has logic that is a
// good candidate for being a `TabDragTarget`, but is tightly coupled with
// responsibilities related to `TabDragContext` lifetime management. We should
// attempt to split out as much of this logic as possible into a new
// `TabDragTarget`.
class TabDragTarget {
 public:
  // An interface exposed to TabDragTarget, allowing interaction with the
  // ongoing drag session.
  class DragController {
   public:
    virtual ~DragController() = default;

    // Detaches the tab corresponding to the index within the current
    // `DragSessionData`. If this is the last tab in the browser, the browser
    // will close.
    // This can only be called once dragging stopped and the referenced
    // tab data must not have been already destroyed.
    virtual std::unique_ptr<tabs::TabModel> DetachTabAtForInsertion(
        int drag_idx) = 0;
    virtual const DragSessionData& GetSessionData() const = 0;
    virtual const TabDragContext* GetAttachedContext() const = 0;
  };

  virtual ~TabDragTarget() = default;

  // Invoked when this becomes the delegate of the drag controller.
  virtual void OnTabDragEntered() = 0;

  // Invoked on each iteration of the drag loop where this is the delegate of
  // the drag controller.
  // Returns the context to attach to, or nullptr if the tabs should be dragged
  // in their own window.
  virtual TabDragContext* OnTabDragUpdated(
      TabDragTarget::DragController& controller,
      const gfx::Point& point_in_screen) = 0;

  // Invoked when this delegate is no longer targeted by the controller.
  virtual void OnTabDragExited(const gfx::Point& point_in_screen) = 0;

  // Notification for the end of a drag, for any reason (e.g. drop, cancel,
  // etc.).
  virtual void OnTabDragEnded() = 0;

  // Indicates whether this delegate should handle a dropped tab.
  virtual bool CanDropTab() = 0;

  // Handles a drop that occurred while this delegate is targeted.
  // This is only invoked if `CanDropTab` returned `true`.
  virtual void HandleTabDrop(DragController& controller) = 0;

  // Registers a callback that gets invoked when this is being destroyed.
  virtual base::CallbackListSubscription RegisterWillDestroyCallback(
      base::OnceClosure callback) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_DRAGGING_TAB_DRAG_TARGET_H_
