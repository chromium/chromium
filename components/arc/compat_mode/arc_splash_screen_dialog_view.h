// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMPAT_MODE_ARC_SPLASH_SCREEN_DIALOG_VIEW_H_
#define COMPONENTS_ARC_COMPAT_MODE_ARC_SPLASH_SCREEN_DIALOG_VIEW_H_

#include "base/callback_forward.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class View;
class Button;
}  // namespace views

namespace arc {

// This class creates a splash screen view as a bubble dialog. The view has a
// transparent background color, with a content box inserted in the middle. It
// also has a close button on the top right corner. This view is intended to be
// inserted into a window. The content container contains a logo, a heading
// text, a message box in vertical alignment.
class ArcSplashScreenDialogView : public views::BubbleDialogDelegateView {
 public:
  // TestApi is used for tests to get internal implementation details.
  class TestApi {
   public:
    explicit TestApi(ArcSplashScreenDialogView* view);
    ~TestApi();

    views::Button* close_button() const;

   private:
    ArcSplashScreenDialogView* const view_;
  };

  ArcSplashScreenDialogView(base::OnceClosure close_callback,
                            aura::Window* parent,
                            views::View* anchor);
  ArcSplashScreenDialogView(const ArcSplashScreenDialogView&) = delete;
  ArcSplashScreenDialogView& operator=(const ArcSplashScreenDialogView&) =
      delete;
  ~ArcSplashScreenDialogView() override;

  // Show a splash screen dialog to advertise resize lock feature
  static void Show(aura::Window* parent);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;

 private:
  void OnLinkClicked();
  void OnCloseButtonClicked();

  base::OnceClosure close_callback_;
  views::Button* close_button_ = nullptr;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_COMPAT_MODE_ARC_SPLASH_SCREEN_DIALOG_VIEW_H_
