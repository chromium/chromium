// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_CURSOR_MANAGER_H_
#define COMPONENTS_INPUT_CURSOR_MANAGER_H_

#include <map>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "components/input/render_widget_host_view_input_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

namespace input {

class RenderWidgetHostViewInput;

// CursorManager coordinates mouse cursors for multiple RenderWidgetHostViews
// on a single page. It is owned by the top-level RenderWidgetHostView and
// calls back to its DisplayCursor method when the cursor needs to change,
// either because the mouse moved over a different view or because a cursor
// update was received for the current view.
class COMPONENT_EXPORT(INPUT) CursorManager
    : public RenderWidgetHostViewInputObserver {
 public:
  explicit CursorManager(RenderWidgetHostViewInput* root);
  ~CursorManager() override;

  // RenderWidgetHostViewInputObserver:
  void OnRenderWidgetHostViewInputDestroyed(
      RenderWidgetHostViewInput* view) override;

  // Called for any RenderWidgetHostView that received an UpdateCursor message
  // from its renderer process.
  void UpdateCursor(RenderWidgetHostViewInput*, const ui::Cursor&);

  // Called when the mouse moves over a different RenderWidgetHostView.
  void UpdateViewUnderCursor(RenderWidgetHostViewInput*);

  // Overrides the view under the cursor while the pointer is over a surface
  // that dispatches mouse events outside RenderWidgetHostInputEventRouter
  // (such as an UnboundedSurfaceWindow). Passing nullptr clears the override
  // and falls back to the view last set by UpdateViewUnderCursor.
  void SetViewUnderCursorOverride(RenderWidgetHostViewInput*);

  // Notification of a RenderWidgetHostView being destroyed, so that its
  // cursor map entry can be removed if it has one. Clears
  // `view_under_cursor_override_` if it matches, and falls back to
  // `root_view_` if `view_under_cursor_` is destroyed.
  void ViewBeingDestroyed(RenderWidgetHostViewInput*);

  // Called by any RenderWidgetHostView before updating the tooltip text to
  // validate that the tooltip text being updated is for the view under the
  // cursor. This is only used for cursor triggered tooltips.
  bool IsViewUnderCursor(RenderWidgetHostViewInput*) const;

  // Disallows custom cursors whose height or width are larger or equal to
  // `max_dimension` DIPs.
  [[nodiscard]] base::ScopedClosureRunner CreateDisallowCustomCursorScope(
      int max_dimension_dips);

  // Accessor for browser tests, enabling verification of the cursor_map_.
  // Returns false if the provided View is not in the map, and outputs
  // the cursor otherwise.
  bool GetCursorForTesting(RenderWidgetHostViewInput*, ui::Cursor&);

  ui::mojom::CursorType GetLastSetCursorTypeForTesting() {
    return last_set_cursor_type_for_testing_;
  }

 private:
  bool IsCursorAllowed(const ui::Cursor&) const;
  void DisallowCustomCursorScopeExpired(int max_dimension_dips);
  RenderWidgetHostViewInput* EffectiveViewUnderCursor() const;
  void UpdateCursor();

  void AddObservation(RenderWidgetHostViewInput* view);
  void RemoveObservation(RenderWidgetHostViewInput* view);

  // Stores the last received cursor from each RenderWidgetHostView.
  std::map<RenderWidgetHostViewInput*, ui::Cursor> cursor_map_;

  base::ScopedMultiSourceObservation<RenderWidgetHostViewInput,
                                     RenderWidgetHostViewInputObserver>
      observations_{this};

  // The view currently underneath the cursor, which corresponds to the cursor
  // currently displayed unless overridden by `view_under_cursor_override_`.
  raw_ptr<RenderWidgetHostViewInput> view_under_cursor_;

  // Optional override set while the pointer is over a surface that routes
  // mouse events outside RenderWidgetHostInputEventRouter.
  raw_ptr<RenderWidgetHostViewInput> view_under_cursor_override_ = nullptr;

  // The root view is the target for DisplayCursor calls whenever the active
  // cursor needs to change.
  const raw_ptr<RenderWidgetHostViewInput> root_view_;

  // Restrictions on the maximum dimension (either width or height) imposed
  // on custom cursors.
  // Restrictions can be created by `CreateDisallowCustomCursorScope`.
  std::vector<int> dimension_restrictions_;

  ui::mojom::CursorType last_set_cursor_type_for_testing_;

  base::WeakPtrFactory<CursorManager> weak_factory_{this};
};

}  // namespace input

#endif  // COMPONENTS_INPUT_CURSOR_MANAGER_H_
