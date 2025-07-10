// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view_delegate.h"

#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/split_tab_visual_data.h"

MultiContentsViewDelegateImpl::MultiContentsViewDelegateImpl(
    TabStripModel& tab_strip_model,
    Browser& browser)
    : tab_strip_model_(tab_strip_model), browser_(browser) {}

void MultiContentsViewDelegateImpl::WebContentsFocused(
    content::WebContents* web_contents) {
  int tab_index = tab_strip_model_->GetIndexOfWebContents(web_contents);
  if (tab_index != TabStripModel::kNoTab) {
    tab_strip_model_->ActivateTabAt(tab_index);
  }
}

void MultiContentsViewDelegateImpl::ReverseWebContents() {
  const int active_index = tab_strip_model_->active_index();

  const std::optional<split_tabs::SplitTabId> split_tab_id =
      tab_strip_model_->GetTabAtIndex(active_index)->GetSplit();

  CHECK(split_tab_id.has_value());
  tab_strip_model_->ReverseTabsInSplit(split_tab_id.value());
}

void MultiContentsViewDelegateImpl::ResizeWebContents(double start_ratio,
                                                      bool done_resizing) {
  const std::optional<split_tabs::SplitTabId> split_tab_id =
      tab_strip_model_->GetActiveTab()->GetSplit();

  CHECK(split_tab_id.has_value());
  tab_strip_model_->UpdateSplitRatio(split_tab_id.value(), start_ratio);

  if (done_resizing) {
    const split_tabs::SplitTabId split_id =
        tab_strip_model_->GetActiveTab()->GetSplit().value();

    SessionService* const session_service =
        SessionServiceFactory::GetForProfile(browser_->profile());

    if (!session_service) {
      return;
    }

    const split_tabs::SplitTabVisualData* visual_data =
        tab_strip_model_->GetSplitData(split_id)->visual_data();
    session_service->SetSplitTabData(browser_->session_id(), split_id,
                                     visual_data);
  }
}

void MultiContentsViewDelegateImpl::HandleLinkDrop(
    MultiContentsDropTargetView::DropSide side,
    const std::vector<GURL>& urls) {
  CHECK(!urls.empty());
  CHECK(!tab_strip_model_->GetActiveTab()->IsSplit());

  // Insert the tab before or after the active tab, according to the drop side.
  const int new_tab_idx =
      tab_strip_model_->active_index() +
      (side == MultiContentsDropTargetView::DropSide::START ? 0 : 1);

  // TODO(crbug.com/406792273): Support entrypoint for vertical splits.
  const split_tabs::SplitTabVisualData split_data(
      split_tabs::SplitTabLayout::kVertical);

  // We currently only support creating a split with one link; i.e., the first
  // link in the provided list.
  tab_strip_model_->delegate()->AddTabAt(urls.front(), new_tab_idx, false);

  tab_strip_model_->AddToNewSplit(
      {new_tab_idx}, split_data,
      split_tabs::SplitTabCreatedSource::kDragAndDropLink);
}
