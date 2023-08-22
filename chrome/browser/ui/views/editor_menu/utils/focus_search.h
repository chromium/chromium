// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_FOCUS_SEARCH_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_FOCUS_SEARCH_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_search.h"

namespace chromeos::editor_menu {

// This class manages the focus traversal order for elements inside
// Quick Answers or Editor Menu related views.
// TODO(siabhijeet): QuickAnswersView is a menu-companion, so ideally should
// avoid disturbing existing focus. Explore other ways for keyboard
// accessibility.
class FocusSearch : public views::FocusSearch, public views::FocusTraversable {
 public:
  using GetFocusableViewsCallback =
      base::RepeatingCallback<std::vector<views::View*>(void)>;

  FocusSearch(views::View* view, const GetFocusableViewsCallback& callback);
  FocusSearch(const FocusSearch&) = delete;
  FocusSearch& operator=(const FocusSearch) = delete;
  ~FocusSearch() override;

  // views::FocusSearch:
  views::View* FindNextFocusableView(
      views::View* starting_view,
      SearchDirection search_direction,
      TraversalDirection traversal_direction,
      StartingViewPolicy check_starting_view,
      AnchoredDialogPolicy can_go_into_anchored_dialog,
      views::FocusTraversable** focus_traversable,
      views::View** focus_traversable_view) override;

  // views::FocusTraversable:
  views::FocusSearch* GetFocusSearch() override;
  views::FocusTraversable* GetFocusTraversableParent() override;
  views::View* GetFocusTraversableParentView() override;

 private:
  const raw_ptr<views::View> view_ = nullptr;
  const GetFocusableViewsCallback get_focusable_views_callback_;
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_FOCUS_SEARCH_H_
