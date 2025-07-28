// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

class BrowserView;
class ContentsWebView;
class MultiContentsViewMiniToolbar;
class ScrimView;

namespace glic {
class GlicBorderView;
}  // namespace glic

namespace new_tab_footer {
class NewTabFooterWebView;
}  // namespace new_tab_footer

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
  ScrimView* GetContentsScrimView() { return contents_scrim_view_; }
  views::View* GetActorOverlayView() { return actor_overlay_view_; }
  glic::GlicBorderView* GetGlicBorderView() { return glic_border_; }
  new_tab_footer::NewTabFooterWebView* GetNewTabFooterView() {
    return new_tab_footer_view_;
  }
  ScrimView* GetInactiveSplitScrimView() { return inactive_split_scrim_view_; }

  void UpdateBorderAndOverlay(bool is_in_split,
                              bool is_active,
                              bool show_scrim);

 private:
  void UpdateBorderRoundedCorners();
  void ClearBorderRoundedCorners();

  // View:
  void ChildVisibilityChanged(View* child) override;

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  bool is_in_split_ = false;

  raw_ptr<ContentsWebView> contents_view_;

  // The view that shows a footer at the bottom of the contents
  // container on new tab pages.
  raw_ptr<new_tab_footer::NewTabFooterWebView> new_tab_footer_view_ = nullptr;
  // Separator between the web contents and the Footer.
  raw_ptr<views::View> new_tab_footer_view_separator_ = nullptr;

  // The scrim view that covers the content area when a tab-modal dialog is
  // open.
  raw_ptr<ScrimView> contents_scrim_view_;

  // Scrim view shown on the inactive side of a split view when the omnibox is
  // focused or site permissions dialogs are showing.
  raw_ptr<ScrimView> inactive_split_scrim_view_ = nullptr;

  // The view that contains the Glic Actor Overlay. The Actor Overlay is a UI
  // overlay that is shown on top of the web contents.
  raw_ptr<views::View> actor_overlay_view_ = nullptr;

  // The glic browser view that renders around the web contents area.
  raw_ptr<glic::GlicBorderView> glic_border_ = nullptr;

  raw_ptr<MultiContentsViewMiniToolbar> mini_toolbar_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_VIEW_H_
