// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LENS_LENS_REGION_SEARCH_INSTRUCTIONS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LENS_LENS_REGION_SEARCH_INSTRUCTIONS_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class ImageButton;
}  // namespace views

namespace lens {

class LensRegionSearchInstructionsView
    : public views::BubbleDialogDelegateView {
  METADATA_HEADER(LensRegionSearchInstructionsView,
                  views::BubbleDialogDelegateView)

 public:
  struct LayoutParams {
    int left_margin;
    gfx::Insets label_button_insets;
    int close_button_margin;
    int vector_icon_size;
    int label_horizontal_distance;
    int vertical_distance;
  };

  LensRegionSearchInstructionsView(views::View* anchor_view,
                                   base::OnceClosure close_callback,
                                   base::OnceClosure escape_callback,
                                   const LayoutParams& layout_params,
                                   int text_message_id);
  LensRegionSearchInstructionsView(const LensRegionSearchInstructionsView&) =
      delete;
  LensRegionSearchInstructionsView& operator=(
      const LensRegionSearchInstructionsView&) = delete;
  ~LensRegionSearchInstructionsView() override;

  // views::View
  void OnThemeChanged() override;

 protected:
  // views::BubbleDialogDelegateView:
  void Init() override;
  gfx::Rect GetBubbleBounds() override;

  // Pointers to views after they have been added to the parent via
  // AddChildView.
  raw_ptr<views::Label> label_;
  raw_ptr<views::ImageButton> constructed_close_button_;

  // Close button needs to be created on construction in order to not store the
  // close callback.
  std::unique_ptr<views::ImageButton> close_button_;

  // Layout parameters provided by caller.
  LayoutParams layout_params_;

  const int text_message_id_;
};
}  // namespace lens
#endif  // CHROME_BROWSER_UI_VIEWS_LENS_LENS_REGION_SEARCH_INSTRUCTIONS_VIEW_H_
