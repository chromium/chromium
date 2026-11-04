// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/child_view_targeter_delegate.h"

#include <algorithm>

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/view.h"

bool ChildViewTargeterDelegate::DoesIntersectRect(const views::View* target,
                                                  const gfx::Rect& rect) const {
  if (!views::ViewTargeterDelegate::DoesIntersectRect(target, rect)) {
    return false;
  }
  return std::any_of(
      target->children().begin(), target->children().end(),
      [target, rect](views::View* child) {
        if (!child->GetVisible() ||
            !child->GetCanProcessEventsWithinSubtree()) {
          return false;
        }
        gfx::RectF rect_in_child_coords_f(rect);
        views::View::ConvertRectToTarget(target, child,
                                         &rect_in_child_coords_f);
        return child->HitTestRect(gfx::ToEnclosingRect(rect_in_child_coords_f));
      });
}
