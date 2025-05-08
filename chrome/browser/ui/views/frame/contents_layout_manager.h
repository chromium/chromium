// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_LAYOUT_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_LAYOUT_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/proposed_layout.h"

// ContentsLayoutManager positions the WebContents and devtools WebContents.
class ContentsLayoutManager : public views::LayoutManagerBase {
 public:
  ContentsLayoutManager(views::View* devtools_view,
                        views::View* devtools_scrim_view,
                        views::View* contents_view,
                        views::View* lens_overlay_view,
                        views::View* scrim_view,
                        views::View* border_view = nullptr,
                        views::View* watermark_view = nullptr,
                        views::View* new_tab_footer_view = nullptr);

  ContentsLayoutManager(const ContentsLayoutManager&) = delete;
  ContentsLayoutManager& operator=(const ContentsLayoutManager&) = delete;

  ~ContentsLayoutManager() override;

  // Sets the contents resizing strategy.
  void SetContentsResizingStrategy(
      const DevToolsContentsResizingStrategy& strategy);

 protected:
  // views::LayoutManagerBase overrides:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

 private:
  const raw_ptr<views::View> devtools_view_;
  const raw_ptr<views::View> devtools_scrim_view_;
  const raw_ptr<views::View> contents_view_;
  const raw_ptr<views::View> lens_overlay_view_;
  const raw_ptr<views::View> scrim_view_;
  const raw_ptr<views::View> border_view_;
  const raw_ptr<views::View> watermark_view_;
  const raw_ptr<views::View> new_tab_footer_view_;

  DevToolsContentsResizingStrategy strategy_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_LAYOUT_MANAGER_H_
