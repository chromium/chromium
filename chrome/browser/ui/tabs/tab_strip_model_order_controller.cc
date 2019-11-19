// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_order_controller.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

///////////////////////////////////////////////////////////////////////////////
// TabStripModelOrderController, public:

TabStripModelOrderController::TabStripModelOrderController(
    TabStripModel* tabstrip)
    : tabstrip_(tabstrip) {
  tabstrip_->AddObserver(this);
}

TabStripModelOrderController::~TabStripModelOrderController() {}

int TabStripModelOrderController::DetermineInsertionIndex(
    ui::PageTransition transition,
    bool foreground) {
  int tab_count = tabstrip_->count();
  if (!tab_count)
    return 0;

  // NOTE: TabStripModel enforces that all non-mini-tabs occur after mini-tabs,
  // so we don't have to check here too.
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK) &&
      tabstrip_->active_index() != -1) {
    if (foreground) {
      // If the page was opened in the foreground by a link click in another
      // tab, insert it adjacent to the tab that opened that link.
      return tabstrip_->active_index() + 1;
    }
    content::WebContents* opener = tabstrip_->GetActiveWebContents();
    // Get the index of the next item opened by this tab, and insert after
    // it...
    int index = tabstrip_->GetIndexOfLastWebContentsOpenedBy(
        opener, tabstrip_->active_index());
    if (index != TabStripModel::kNoTab)
      return index + 1;
    // Otherwise insert adjacent to opener...
    return tabstrip_->active_index() + 1;
  }
  // In other cases, such as Ctrl+T, open at the end of the strip.
  return tabstrip_->count();
}

base::Optional<int> TabStripModelOrderController::DetermineNewSelectedIndex(
    int removing_index) const {
  DCHECK(tabstrip_->ContainsIndex(removing_index));

  // The case where the closed tab is inactive is handled directly in
  // TabStripModel.
  if (removing_index != tabstrip_->active_index())
    return base::nullopt;

  // The case where multiple tabs are selected is handled directly in
  // TabStripModel.
  if (tabstrip_->selection_model().size() > 1)
    return base::nullopt;

  content::WebContents* parent_opener =
      tabstrip_->GetOpenerOfWebContentsAt(removing_index);
  // First see if the index being removed has any "child" tabs. If it does, we
  // want to select the first that child opened, not the next tab opened by the
  // removed tab.
  content::WebContents* removed_contents =
      tabstrip_->GetWebContentsAt(removing_index);
  // The parent opener should never be the same as the controller being removed.
  DCHECK(parent_opener != removed_contents);
  int index = tabstrip_->GetIndexOfNextWebContentsOpenedBy(removed_contents,
                                                           removing_index);
  if (index != TabStripModel::kNoTab)
    return GetValidIndex(index, removing_index);

  if (parent_opener) {
    // If the tab has an opener, shift selection to the next tab with the same
    // opener.
    int index = tabstrip_->GetIndexOfNextWebContentsOpenedBy(parent_opener,
                                                             removing_index);
    if (index != TabStripModel::kNoTab)
      return GetValidIndex(index, removing_index);

    // If we can't find another tab with the same opener, fall back to the
    // opener itself.
    index = tabstrip_->GetIndexOfWebContents(parent_opener);
    if (index != TabStripModel::kNoTab)
      return GetValidIndex(index, removing_index);
  }

  // If closing a grouped tab, return a tab that is still in the group, if any.
  const base::Optional<TabGroupId> current_group =
      tabstrip_->GetTabGroupForTab(removing_index);
  if (current_group.has_value()) {
    // Match the default behavior below: prefer the tab to the right.
    const base::Optional<TabGroupId> right_group =
        tabstrip_->GetTabGroupForTab(removing_index + 1);
    if (current_group == right_group)
      return removing_index;

    const base::Optional<TabGroupId> left_group =
        tabstrip_->GetTabGroupForTab(removing_index - 1);
    if (current_group == left_group)
      return removing_index - 1;
  }

  // By default, return the tab on the right, unless this is the last tab.
  if (removing_index >= (tabstrip_->count() - 1))
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
    int index = tabstrip_->GetIndexOfWebContents(old_contents);
    if (index != TabStripModel::kNoTab) {
      old_opener = tabstrip_->GetOpenerOfWebContentsAt(index);

      // Forget the opener relationship if it needs to be reset whenever the
      // active tab changes (see comment in TabStripModel::AddWebContents, where
      // the flag is set).
      if (tabstrip_->ShouldResetOpenerOnActiveTabChange(old_contents))
        tabstrip_->ForgetOpener(old_contents);
    }
  }
  content::WebContents* new_opener =
      tabstrip_->GetOpenerOfWebContentsAt(selection.new_model.active());

  if ((reason & CHANGE_REASON_USER_GESTURE) && new_opener != old_opener &&
      ((old_contents == nullptr && new_opener == nullptr) ||
       new_opener != old_contents) &&
      ((new_contents == nullptr && old_opener == nullptr) ||
       old_opener != new_contents)) {
    tabstrip_->ForgetAllOpeners();
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
