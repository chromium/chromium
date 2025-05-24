// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LENS_OVERLAY_HOMEWORK_PAGE_ACTION_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LENS_OVERLAY_HOMEWORK_PAGE_ACTION_ICON_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class BrowserWindowInterface;
class ScopedWindowCallToAction;

class LensOverlayHomeworkPageActionIconView : public PageActionIconView {
  METADATA_HEADER(LensOverlayHomeworkPageActionIconView, PageActionIconView)

 public:
  LensOverlayHomeworkPageActionIconView(
      IconLabelBubbleView::Delegate* parent_delegate,
      Delegate* delegate,
      BrowserWindowInterface* browser);
  ~LensOverlayHomeworkPageActionIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

  void ExecuteWithKeyboardSourceForTesting();

 protected:
  // PageActionIconView:
  void UpdateImpl() override;

 private:
  bool ShouldShow();
  void ShowCallToAction();

  const raw_ptr<BrowserWindowInterface> browser_;

  std::unique_ptr<ScopedWindowCallToAction> scoped_window_call_to_action_ptr_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LENS_OVERLAY_HOMEWORK_PAGE_ACTION_ICON_VIEW_H_
