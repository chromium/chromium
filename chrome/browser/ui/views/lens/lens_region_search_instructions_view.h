// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LENS_LENS_REGION_SEARCH_INSTRUCTIONS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LENS_LENS_REGION_SEARCH_INSTRUCTIONS_VIEW_H_

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class ImageButton;
}  // namespace views

namespace lens {

class LensRegionSearchInstructionsView
    : public views::BubbleDialogDelegateView {
 public:
  LensRegionSearchInstructionsView(views::View* anchor_view,
                                   base::OnceClosure close_callback,
                                   base::OnceClosure escape_callback);
  LensRegionSearchInstructionsView(const LensRegionSearchInstructionsView&) =
      delete;
  LensRegionSearchInstructionsView& operator=(
      const LensRegionSearchInstructionsView&) = delete;
  ~LensRegionSearchInstructionsView() override;

 protected:
  // views::BubbleDialogDelegateView:
  void Init() override;
  gfx::Rect GetBubbleBounds() override;

  // Close button needs to be created on construction in order to not store the
  // close callback.
  std::unique_ptr<views::ImageButton> close_button_;
};
}  // namespace lens
#endif  // CHROME_BROWSER_UI_VIEWS_LENS_LENS_REGION_SEARCH_INSTRUCTIONS_VIEW_H_
