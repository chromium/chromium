// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_TWO_ORIGINS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_TWO_ORIGINS_VIEW_H_

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"

// Bubble that prompts the user to grant or deny a permission request from from
// a pair of origins.
//
// ----------------------------------------------
// |                                       [ X ]|
// | Prompt title mentioning the two origins    |
// | ------------------------------------------ |
// | Favicons from the two origins              |
// | ------------------------------------------ |
// | Extra text                                 |
// | ------------------------------------------ |
// |                        [ Block ] [ Allow ] |
// ----------------------------------------------
class PermissionPromptBubbleTwoOriginsView
    : public PermissionPromptBubbleBaseView {
 public:
  PermissionPromptBubbleTwoOriginsView(
      Browser* browser,
      base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
      base::TimeTicks permission_requested_time,
      PermissionPromptStyle prompt_style);
  PermissionPromptBubbleTwoOriginsView(
      const PermissionPromptBubbleTwoOriginsView&) = delete;
  PermissionPromptBubbleTwoOriginsView& operator=(
      const PermissionPromptBubbleTwoOriginsView&) = delete;
  ~PermissionPromptBubbleTwoOriginsView() override;

  // views::BubbleDialogDelegateView:
  void AddedToWidget() override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_BUBBLE_TWO_ORIGINS_VIEW_H_
