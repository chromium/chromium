// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_ZOOM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_ZOOM_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

// View for the zoom icon in the Omnibox.
class ZoomView : public PageActionIconView {
  METADATA_HEADER(ZoomView, PageActionIconView)

 public:
  // Clicking on the ZoomView shows a ZoomBubbleView, which requires the current
  // WebContents. Because the current WebContents changes as the user switches
  // tabs, a LocationBarView::Delegate is supplied to queried for the current
  // WebContents when needed.
  ZoomView(IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
           PageActionIconView::Delegate* page_action_icon_delegate);
  ZoomView(const ZoomView&) = delete;
  ZoomView& operator=(const ZoomView&) = delete;
  ~ZoomView() override;

  // Updates the image and its tooltip appropriately, hiding or showing the icon
  // as needed.
  void ZoomChangedForActiveTab(bool can_show_bubble);

 protected:
  // PageActionIconView:
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  views::BubbleDialogDelegate* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  bool ShouldBeVisible(bool can_show_bubble) const;
  bool HasAssociatedBubble() const;

  raw_ptr<const gfx::VectorIcon> icon_ = nullptr;

  int current_zoom_percent_ = 100;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_ZOOM_VIEW_H_
