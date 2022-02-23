// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_order_controller.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

///////////////////////////////////////////////////////////////////////////////
// TabStripModelOrderController, public:

TabStripModelOrderController::TabStripModelOrderController(TabStripModel* model)
    : model_(model) {
  model_->AddObserver(this);
}

TabStripModelOrderController::~TabStripModelOrderController() {}

int TabStripModelOrderController::DetermineInsertionIndex(
    ui::PageTransition transition,
    bool foreground) {
  int tab_count = model_->count();
  if (!tab_count)
    return 0;

  // NOTE: TabStripModel enforces that all non-mini-tabs occur after mini-tabs,
  // so we don't have to check here too.
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK) &&
      model_->active_index() != -1) {
    if (foreground) {
      // If the page was opened in the foreground by a link click in another
      // tab, insert it adjacent to the tab that opened that link.
      return model_->active_index() + 1;
    }
    content::WebContents* const opener = model_->GetActiveWebContents();
    // Figure out the last tab opened by the current tab.
    const int index = model_->GetIndexOfLastWebContentsOpenedBy(
        opener, model_->active_index());
    // If no such tab exists, simply place next to the current tab.
    if (index == TabStripModel::kNoTab)
      return model_->active_index() + 1;

    // Normally we'd add the tab immediately after the most recent tab
    // associated with `opener`. However, if there is a group discontinuity
    // between the active tab and where we'd like to place the tab, we'll place
    // it just before the discontinuity instead (see crbug.com/1246421).
    const auto opener_group = model_->GetTabGroupForTab(model_->active_index());
    for (int i = model_->active_index() + 1; i <= index; ++i) {
      // Insert before the first tab that differs in group.
      if (model_->GetTabGroupForTab(i) != opener_group)
        return i;
    }
    // If there is no discontinuity, add after the last tab already associated
    // with the opener.
    return index + 1;
  }
  // In other cases, such as Ctrl+T, open at the end of the strip.
  return model_->count();
}

absl::optional<int> TabStripModelOrderController::DetermineNewSelectedIndex(
    int removing_index) const {
  DCHECK(model_->ContainsIndex(removing_index));

  // The case where the closed tab is inactive is handled directly in
  // TabStripModel.
  if (removing_index != model_->active_index())
    return absl::nullopt;

  // The case where multiple tabs are selected is handled directly in
  // TabStripModel.
  if (model_->selection_model().size() > 1)
    return absl::nullopt;

  content::WebContents* parent_opener =
      model_->GetOpenerOfWebContentsAt(removing_index);
  // First see if the index being removed has any "child" tabs. If it does, we
  // want to select the first that child opened, not the next tab opened by the
  // removed tab.
  content::WebContents* removed_contents =
      model_->GetWebContentsAt(removing_index);
  // The parent opener should never be the same as the controller being removed.
  DCHECK(parent_opener != removed_contents);
  int index = model_->GetIndexOfNextWebContentsOpenedBy(removed_contents,
                                                        removing_index);
  if (index != TabStripModel::kNoTab && !model_->IsTabCollapsed(index))
    return GetValidIndex(index, removing_index);

  if (parent_opener) {
    // If the tab has an opener, shift selection to the next tab with the same
    // opener.
    index = model_->GetIndexOfNextWebContentsOpenedBy(parent_opener,
                                                      removing_index);
    if (index != TabStripModel::kNoTab && !model_->IsTabCollapsed(index))
      return GetValidIndex(index, removing_index);

    // If we can't find another tab with the same opener, fall back to the
    // opener itself.
    index = model_->GetIndexOfWebContents(parent_opener);
    if (index != TabStripModel::kNoTab && !model_->IsTabCollapsed(index))
      return GetValidIndex(index, removing_index);
  }

  // If closing a grouped tab, return a tab that is still in the group, if any.
  const absl::optional<tab_groups::TabGroupId> current_group =
      model_->GetTabGroupForTab(removing_index);
  if (current_group.has_value()) {
    // Match the default behavior below: prefer the tab to the right.
    const absl::optional<tab_groups::TabGroupId> right_group =
        model_->GetTabGroupForTab(removing_index + 1);
    if (current_group == right_group)
      return removing_index;

    const absl::optional<tab_groups::TabGroupId> left_group =
        model_->GetTabGroupForTab(removing_index - 1);
    if (current_group == left_group)
      return removing_index - 1;
  }

  // At this point, the tab detaching is either not inside a group, or the last
  // tab in the group. If there are any tabs in a not collapsed group,
  // |GetNextExpandedActiveTab()| will return the index of that tab.
  absl::optional<int> next_available =
      model_->GetNextExpandedActiveTab(removing_index, absl::nullopt);
  if (next_available.has_value())
    return GetValidIndex(next_available.value(), removing_index);

  // By default, return the tab on the right, unless this is the last tab.
  // Reaching this point means there are no other tabs in an uncollapsed group.
  // The tab at the specified index will become automatically expanded by the
  // caller.
  if (removing_index >= (model_->count() - 1))
    return removing_index - 1;

  return removing_index;
}

void TabStripModelOrderController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed() || tab_strip_model->empty())
    return;

  content::WebContents* old_contents = selection.old_contents;
  content::WebContents* new_contents = selection.new_contents;
  content::WebContents* old_opener = nullptr;
  int reason = selection.reason;

  if (old_contents) {
    int index = model_->GetIndexOfWebContents(old_contents);
    if (index != TabStripModel::kNoTab) {
      old_opener = model_->GetOpenerOfWebContentsAt(index);

      // Forget the opener relationship if it needs to be reset whenever the
      // active tab changes (see comment in TabStripModel::AddWebContents, where
      // the flag is set).
      if (model_->ShouldResetOpenerOnActiveTabChange(old_contents))
        model_->ForgetOpener(old_contents);
    }
  }
  content::WebContents* new_opener =
      model_->GetOpenerOfWebContentsAt(selection.new_model.active());

  if ((reason & CHANGE_REASON_USER_GESTURE) && new_opener != old_opener &&
      ((old_contents == nullptr && new_opener == nullptr) ||
       new_opener != old_contents) &&
      ((new_contents == nullptr && old_opener == nullptr) ||
       old_opener != new_contents)) {
    model_->ForgetAllOpeners();
  }
}

///////////////////////////////////////////////////////////////////////////////
// TabStripModelOrderController, private:

int TabStripModelOrderController::GetValidIndex(int index,
                                                int removing_index) const {
  if (removing_index < index)
    index = std::max(0, index - 1);
  return index;
}
