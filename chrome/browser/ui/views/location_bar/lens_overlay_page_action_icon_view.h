// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LENS_OVERLAY_PAGE_ACTION_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LENS_OVERLAY_PAGE_ACTION_ICON_VIEW_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class Browser;

class LensOverlayPageActionIconView : public PageActionIconView {
  METADATA_HEADER(LensOverlayPageActionIconView, PageActionIconView)

 public:
  LensOverlayPageActionIconView(
      Browser* browser,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  LensOverlayPageActionIconView(const LensOverlayPageActionIconView&) = delete;
  LensOverlayPageActionIconView& operator=(
      const LensOverlayPageActionIconView&) = delete;
  ~LensOverlayPageActionIconView() override;

  void set_update_callback_for_testing(base::OnceClosure update_callback) {
    update_callback_for_testing_ = std::move(update_callback);
  }

  void execute_with_keyboard_source_for_testing() {
    CHECK(GetVisible());
    OnExecuting(EXECUTE_SOURCE_KEYBOARD);
  }

 protected:
  // PageActionIconView:
  bool ShouldShowLabel() const override;
  void UpdateImpl() override;
  void OnExecuting(PageActionIconView::ExecuteSource source) override;
  views::BubbleDialogDelegate* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  // Preferred sizes for this view with and without the label set.
  gfx::Size preferred_size_with_label_;
  gfx::Size preferred_size_without_label_;

  // Controls the `ShouldShowLabel` behavior.
  bool should_show_label_ = true;

  // A controller for taking a full page screenshot and opening Lens in a new
  // tab if needed for accessibility.
  std::unique_ptr<lens::LensRegionSearchController>
      lens_region_search_controller_;

  raw_ptr<Browser> browser_;
  base::OnceClosure update_callback_for_testing_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LENS_OVERLAY_PAGE_ACTION_ICON_VIEW_H_
