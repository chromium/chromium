// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_ICON_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class Browser;

// A search icon that appears when the side search feature is available. Opens
// the side panel to the available SRP.
class SideSearchIconView : public PageActionIconView,
                           public TabStripModelObserver {
  METADATA_HEADER(SideSearchIconView, PageActionIconView)

 public:
  SideSearchIconView(IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
                     PageActionIconView::Delegate* page_action_icon_delegate,
                     Browser* browser);
  SideSearchIconView(const SideSearchIconView&) = delete;
  SideSearchIconView& operator=(const SideSearchIconView&) = delete;
  ~SideSearchIconView() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  void SetLabelVisibilityForTesting(bool visible);

  bool IsLabelVisibleForTesting() const;

 protected:
  // PageActionIconView:
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  views::BubbleDialogDelegate* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  ui::ImageModel GetSizedIconImage(int size) const override;
  void AnimationProgressed(const gfx::Animation* animation) override;

 private:
  // Hides the page action label text and cancels the animation if necessary.
  void HidePageActionLabel();

  // Called when the side panel entrypoint becomes available. Animates-in the
  // page action label if appropriate according to the enabled IPH
  // configuration. Returns true if successfully shown.
  bool MaybeShowPageActionLabel();

  raw_ptr<Browser> browser_ = nullptr;

  // Subscription to change notifications to the default search icon source.
  base::CallbackListSubscription icon_changed_subscription_;

  // Animates out the side search icon label after a fixed period of time. This
  // keeps the label visible for long enough to give users an opportunity to
  // read the label text.
  base::OneShotTimer animate_out_timer_;

  // Boolean that tracks whether we should extend the duration for which the
  // label is shown when it animates in.
  bool should_extend_label_shown_duration_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_SIDE_SEARCH_ICON_VIEW_H_
