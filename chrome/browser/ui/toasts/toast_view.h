// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOASTS_TOAST_VIEW_H_
#define CHROME_BROWSER_UI_TOASTS_TOAST_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace toasts {
// The view for toasts.
class ToastView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(ToastView, views::BubbleDialogDelegateView)

 public:
  ToastView(views::View* anchor_view,
            const std::u16string& toast_text,
            const gfx::VectorIcon& icon);
  ~ToastView() override;

  // views::BubbleDialogDelegateView:
  void Init() override;

  views::Label* label_for_testing() { return label_; }

 protected:
  // views::BubbleDialogDelegateView:
  gfx::Rect GetBubbleBounds() override;
  void OnThemeChanged() override;
  std::u16string GetAccessibleWindowTitle() const override;

 private:
  const std::u16string toast_text_;
  const raw_ref<const gfx::VectorIcon> icon_;

  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::ImageView> icon_view_ = nullptr;
};

}  // namespace toasts

#endif  // CHROME_BROWSER_UI_TOASTS_TOAST_VIEW_H_
