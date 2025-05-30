// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_VIEW_H_

#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

class BrowserView;
class ContentsWebView;
class MultiContentsViewMiniToolbar;

// ContentsContainerView is owned by MultiContentsView and holds the
// ContentsWebView and the outlines and minitoolbar when in split view.
class ContentsContainerView : public views::View, public views::LayoutDelegate {
  METADATA_HEADER(ContentsContainerView, views::View)
 public:
  explicit ContentsContainerView(BrowserView* browser_view);
  ContentsContainerView(ContentsContainerView&) = delete;
  ContentsContainerView& operator=(const ContentsContainerView&) = delete;
  ~ContentsContainerView() override = default;

  ContentsWebView* GetContentsView() { return contents_view_; }
  MultiContentsViewMiniToolbar* GetMiniToolbar() { return mini_toolbar_; }

  void UpdateBorderAndOverlay(bool is_in_split, bool is_active);

 private:
  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  raw_ptr<ContentsWebView> contents_view_;
  raw_ptr<MultiContentsViewMiniToolbar> mini_toolbar_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_VIEW_H_
