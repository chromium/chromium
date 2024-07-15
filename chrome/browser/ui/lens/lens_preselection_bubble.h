// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_PRESELECTION_BUBBLE_H_
#define CHROME_BROWSER_UI_LENS_LENS_PRESELECTION_BUBBLE_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"

namespace lens {

// The lens preselection bubble gives users info on how to interact with the
// lens overlay.
class LensPreselectionBubble : public views::BubbleDialogDelegateView {
  METADATA_HEADER(LensPreselectionBubble, views::BubbleDialogDelegateView)

 public:
  explicit LensPreselectionBubble(views::View* anchor_view);
  ~LensPreselectionBubble() override;

  // views::BubbleDialogDelegateView:
  void Init() override;

 protected:
  // views::BubbleDialogDelegateView:
  gfx::Rect GetBubbleBounds() override;
  void OnThemeChanged() override;

 private:
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::ImageView> icon_view_ = nullptr;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_PRESELECTION_BUBBLE_H_
