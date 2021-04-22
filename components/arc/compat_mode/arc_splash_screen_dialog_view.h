// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMPAT_MODE_ARC_SPLASH_SCREEN_DIALOG_VIEW_H_
#define COMPONENTS_ARC_COMPAT_MODE_ARC_SPLASH_SCREEN_DIALOG_VIEW_H_

#include "ui/views/controls/button/button.h"

namespace arc {

// This class creates a splash screen view looks like a dialog. The view has a
// transparent background color, with a content box inserted in the middle. It
// also has a close button on the top right corner. This view is intended to be
// inserted into a window. The content container contains a logo, a heading
// text, a message box in vertical alignment.
class ArcSplashScreenDialogView : public views::View {
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

  explicit ArcSplashScreenDialogView(
      views::Button::PressedCallback close_callback);
  ArcSplashScreenDialogView(const ArcSplashScreenDialogView&) = delete;
  ArcSplashScreenDialogView& operator=(const ArcSplashScreenDialogView&) =
      delete;
  ~ArcSplashScreenDialogView() override;

 private:
  void OnLinkClicked();

  views::Button* close_button_ = nullptr;
};

// Build a splash screen dialog view to advertise resize lock feature
std::unique_ptr<ArcSplashScreenDialogView> BuildSplashScreenDialogView(
    views::Button::PressedCallback close_callback);

}  // namespace arc

#endif  // COMPONENTS_ARC_COMPAT_MODE_ARC_SPLASH_SCREEN_DIALOG_VIEW_H_
