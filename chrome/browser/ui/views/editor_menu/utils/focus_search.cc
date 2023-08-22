// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/editor_menu/utils/focus_search.h"

#include "ui/views/focus/focus_search.h"

namespace chromeos::editor_menu {

FocusSearch::FocusSearch(views::View* view,
                         const GetFocusableViewsCallback& callback)
    : views::FocusSearch(/*root=*/view,
                         /*cycle=*/true,
                         /*accessibility_mode=*/true),
      view_(view),
      get_focusable_views_callback_(callback) {}

FocusSearch::~FocusSearch() = default;

views::View* FocusSearch::FindNextFocusableView(
    views::View* starting_view,
    SearchDirection search_direction,
    TraversalDirection traversal_direction,
    StartingViewPolicy check_starting_view,
    AnchoredDialogPolicy can_go_into_anchored_dialog,
    views::FocusTraversable** focus_traversable,
    views::View** focus_traversable_view) {
  DCHECK_EQ(root(), view_);

  // The callback provided by |view_| polls the currently focusable views.
  auto focusable_views = get_focusable_views_callback_.Run();
  if (focusable_views.empty()) {
    return nullptr;
  }

  int delta =
      search_direction == FocusSearch::SearchDirection::kForwards ? 1 : -1;
  int focusable_views_size = static_cast<int>(focusable_views.size());
  for (int i = 0; i < focusable_views_size; ++i) {
    // If current view from the set is found to be focused, return the view
    // next (or previous) to it as next focusable view.
    if (focusable_views[i] == starting_view) {
      int next_index =
          (i + delta + focusable_views_size) % focusable_views_size;
      return focusable_views[next_index];
    }
  }

  // Case when none of the views are already focused.
  return (search_direction == FocusSearch::SearchDirection::kForwards)
             ? focusable_views.front()
             : focusable_views.back();
}

views::FocusSearch* FocusSearch::GetFocusSearch() {
  return this;
}

views::FocusTraversable* FocusSearch::GetFocusTraversableParent() {
  return nullptr;
}

views::View* FocusSearch::GetFocusTraversableParentView() {
  return nullptr;
}

}  // namespace chromeos::editor_menu
