// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_CURSOR_MANAGER_H_
#define COMPONENTS_INPUT_CURSOR_MANAGER_H_

#include <map>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/component_export.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

namespace input {

class RenderWidgetHostViewInput;

// CursorManager coordinates mouse cursors for multiple RenderWidgetHostViews
// on a single page. It is owned by the top-level RenderWidgetHostView and
// calls back to its DisplayCursor method when the cursor needs to change,
// either because the mouse moved over a different view or because a cursor
// update was received for the current view.
class COMPONENT_EXPORT(INPUT) CursorManager {
 public:
  explicit CursorManager(RenderWidgetHostViewInput* root);
  ~CursorManager();

  // Called for any RenderWidgetHostView that received an UpdateCursor message
  // from its renderer process.
  void UpdateCursor(RenderWidgetHostViewInput*, const ui::Cursor&);

  // Called when the mouse moves over a different RenderWidgetHostView.
  void UpdateViewUnderCursor(RenderWidgetHostViewInput*);

  // Notification of a RenderWidgetHostView being destroyed, so that its
  // cursor map entry can be removed if it has one. If it is the current
  // view_under_cursor_, then the root_view_'s cursor will be displayed.
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
  void UpdateCursor();

  // Stores the last received cursor from each RenderWidgetHostView.
  std::map<RenderWidgetHostViewInput*, ui::Cursor> cursor_map_;

  // The view currently underneath the cursor, which corresponds to the cursor
  // currently displayed.
  // TODO(crbug.com/324115585): Fix FlakyDanglingUntriaged.
  raw_ptr<RenderWidgetHostViewInput, FlakyDanglingUntriaged>
      view_under_cursor_;

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
