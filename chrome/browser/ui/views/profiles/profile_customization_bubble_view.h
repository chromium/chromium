// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CUSTOMIZATION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CUSTOMIZATION_BUBBLE_VIEW_H_

#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class View;
}  // namespace views

class Profile;

// This bubble is implemented as a WebUI page rendered inside a native bubble.
class ProfileCustomizationBubbleView : public views::BubbleDialogDelegateView {
 public:
  ~ProfileCustomizationBubbleView() override;

  ProfileCustomizationBubbleView(const ProfileCustomizationBubbleView& other) =
      delete;
  ProfileCustomizationBubbleView& operator=(
      const ProfileCustomizationBubbleView& other) = delete;

  static void CreateBubble(Profile* profile, views::View* anchor_view);

 private:
  ProfileCustomizationBubbleView(Profile* profile, views::View* anchor_view);

  // Called when the "Done" button is clicked in the inner WebUI.
  void OnDoneButtonClicked();
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CUSTOMIZATION_BUBBLE_VIEW_H_
