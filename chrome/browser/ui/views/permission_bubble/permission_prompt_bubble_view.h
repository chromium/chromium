// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_style.h"
#include "components/permissions/permission_prompt.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Browser;

// Bubble that prompts the user to grant or deny a permission request from a
// website.
class PermissionPromptBubbleView : public views::BubbleDialogDelegateView {
 public:
  PermissionPromptBubbleView(Browser* browser,
                             permissions::PermissionPrompt::Delegate* delegate,
                             base::TimeTicks permission_requested_time,
                             PermissionPromptStyle prompt_style);
  ~PermissionPromptBubbleView() override;

  void Show();

  // Anchors the bubble to the view or rectangle returned from
  // bubble_anchor_util::GetPageInfoAnchorConfiguration.
  void UpdateAnchorPosition();

  void SetPromptStyle(PermissionPromptStyle prompt_style);

  // views::BubbleDialogDelegateView:
  void AddedToWidget() override;
  bool ShouldShowCloseButton() const override;
  base::string16 GetAccessibleWindowTitle() const override;
  base::string16 GetWindowTitle() const override;

  void AcceptPermission();
  void AcceptPermissionThisTime();
  void DenyPermission();
  void ClosingPermission();

 private:
  // Holds the string to be displayed as the origin of the permission prompt,
  // and whether or not that string is an origin.
  struct DisplayNameOrOrigin {
    base::string16 name_or_origin;
    bool is_origin;
  };

  std::vector<permissions::PermissionRequest*> GetVisibleRequests();
  bool ShouldShowPermissionRequest(permissions::PermissionRequest* request);
  void AddPermissionRequestLine(permissions::PermissionRequest* request);

  // Returns the origin to be displayed in the permission prompt. May return
  // a non-origin, e.g. extension URLs use the name of the extension.
  DisplayNameOrOrigin GetDisplayNameOrOrigin() const;

  // Get extra information to display for the permission, if any.
  base::Optional<base::string16> GetExtraText() const;

  // Record UMA Permissions.Prompt.TimeToDecision metric.
  void RecordDecision();

  // Determines whether the current request should also display an
  // "Allow only this time" option in addition to the "Allow on every visit"
  // option.
  bool ShouldShowAllowThisTimeButton() const;

  Browser* const browser_;
  permissions::PermissionPrompt::Delegate* const delegate_;

  // List of permission requests that should be visible in the bubble.
  std::vector<permissions::PermissionRequest*> visible_requests_;

  // The requesting domain's name or origin.
  const DisplayNameOrOrigin name_or_origin_;

  base::TimeTicks permission_requested_time_;

  DISALLOW_COPY_AND_ASSIGN(PermissionPromptBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_BUBBLE_VIEW_H_
