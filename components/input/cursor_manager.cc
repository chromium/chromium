// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/cursor_manager.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "components/input/render_widget_host_view_input.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace input {

CursorManager::CursorManager(RenderWidgetHostViewInput* root)
    : view_under_cursor_(root), root_view_(root) {}

CursorManager::~CursorManager() = default;

void CursorManager::UpdateCursor(RenderWidgetHostViewInput* view,
                                 const ui::Cursor& cursor) {
  cursor_map_[view] = cursor;
  if (view == view_under_cursor_) {
    UpdateCursor();
  }
}

void CursorManager::UpdateViewUnderCursor(
    RenderWidgetHostViewInput* view) {
  if (view == view_under_cursor_)
    return;

  // Whenever we switch from one view to another, clear the tooltip: as the
  // mouse moves, the view now controlling the cursor will send a new tooltip,
  // though this is only guaranteed if the view's tooltip is non-empty, so
  // clearing here is important. Tooltips sent from the previous view will be
  // ignored.
  root_view_->UpdateTooltip(std::u16string());
  view_under_cursor_ = view;
  UpdateCursor();
}

void CursorManager::ViewBeingDestroyed(RenderWidgetHostViewInput* view) {
  cursor_map_.erase(view);

  // If the view right under the mouse is going away, use the root's cursor
  // until UpdateViewUnderCursor is called again.
  if (view == view_under_cursor_ && view != root_view_)
    UpdateViewUnderCursor(root_view_);
}

bool CursorManager::IsViewUnderCursor(
    RenderWidgetHostViewInput* view) const {
  return view == view_under_cursor_;
}

base::ScopedClosureRunner CursorManager::CreateDisallowCustomCursorScope(
    int max_dimension_dips) {
  const ui::Cursor& target_cursor = cursor_map_[view_under_cursor_];
  const bool cursor_allowed_before = IsCursorAllowed(target_cursor);
  dimension_restrictions_.push_back(max_dimension_dips);

  // If the new restriction eliminates the cursor under the current view, update
  // it.
  if (cursor_allowed_before && !IsCursorAllowed(target_cursor)) {
    UpdateCursor();
  }

  return base::ScopedClosureRunner(
      base::BindOnce(&CursorManager::DisallowCustomCursorScopeExpired,
                     weak_factory_.GetWeakPtr(), max_dimension_dips));
}

bool CursorManager::GetCursorForTesting(RenderWidgetHostViewInput* view,
                                        ui::Cursor& cursor) {
  if (cursor_map_.find(view) == cursor_map_.end()) {
    return false;
  }

  cursor = cursor_map_[view];
  return true;
}

bool CursorManager::IsCursorAllowed(const ui::Cursor& cursor) const {
  if (cursor.type() != ui::mojom::CursorType::kCustom ||
      dimension_restrictions_.empty()) {
    return true;
  }

  const int max_dimension_dips = base::ranges::min(dimension_restrictions_);
  const gfx::Size size_in_dip = gfx::ScaleToCeiledSize(
      gfx::SkISizeToSize(cursor.custom_bitmap().dimensions()),
      1 / cursor.image_scale_factor());

  return std::max(size_in_dip.width(), size_in_dip.height()) <
         max_dimension_dips;
}

void CursorManager::DisallowCustomCursorScopeExpired(int max_dimension_dips) {
  const ui::Cursor& target_cursor = cursor_map_[view_under_cursor_];
  const bool cursor_allowed_before = IsCursorAllowed(target_cursor);

  auto it = base::ranges::find(dimension_restrictions_, max_dimension_dips);
  CHECK(it != dimension_restrictions_.end());
  dimension_restrictions_.erase(it);

  if (!cursor_allowed_before && IsCursorAllowed((target_cursor))) {
    UpdateCursor();
  }
}

void CursorManager::UpdateCursor() {
  ui::Cursor cursor(ui::mojom::CursorType::kPointer);

  auto it = cursor_map_.find(view_under_cursor_);
  if (it != cursor_map_.end() && IsCursorAllowed(it->second)) {
    cursor = it->second;
  }

  last_set_cursor_type_for_testing_ = cursor.type();

  root_view_->DisplayCursor(cursor);
}

}  // namespace input
