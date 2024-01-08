// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CUSTOMIZATION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CUSTOMIZATION_BUBBLE_VIEW_H_

#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/webui/signin/profile_customization_handler.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class View;
}  // namespace views

class Browser;
class Profile;

// This bubble is implemented as a WebUI page rendered inside a native bubble.
// After the bubble is closed, a IPH for profile switching may be shown.
class ProfileCustomizationBubbleView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(ProfileCustomizationBubbleView,
                  views::BubbleDialogDelegateView)

 public:
  ProfileCustomizationBubbleView(const ProfileCustomizationBubbleView& other) =
      delete;
  ProfileCustomizationBubbleView& operator=(
      const ProfileCustomizationBubbleView& other) = delete;
  ~ProfileCustomizationBubbleView() override;

  // Creates and shows the bubble.
  static ProfileCustomizationBubbleView* CreateBubble(Browser* browser,
                                                      views::View* anchor_view);

 private:
  FRIEND_TEST_ALL_PREFIXES(ProfileBubbleInteractiveUiTest,
                           CustomizationBubbleFocus);
  FRIEND_TEST_ALL_PREFIXES(ProfileCustomizationBubbleBrowserTest, IPH);

  ProfileCustomizationBubbleView(Profile* profile, views::View* anchor_view);

  // Called when the "Done" or "Skip" button is clicked in the inner WebUI.
  void OnCompletionButtonClicked(
      ProfileCustomizationHandler::CustomizationResult customization_result);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_CUSTOMIZATION_BUBBLE_VIEW_H_
