// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_REQUEST_CHIP_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_REQUEST_CHIP_H_

#include "chrome/browser/ui/views/location_bar/permission_chip.h"

class Browser;

// A chip view shown in the location bar to notify user about a permission
// request. Shows a permission bubble on click.
class PermissionRequestChip : public PermissionChip {
 public:
  METADATA_HEADER(PermissionRequestChip);
  explicit PermissionRequestChip(
      Browser* browser,
      permissions::PermissionPrompt::Delegate* delegate);
  PermissionRequestChip(const PermissionRequestChip& chip) = delete;
  PermissionRequestChip& operator=(const PermissionRequestChip& chip) = delete;
  ~PermissionRequestChip() override;

  // PermissionChip:
  void OpenBubble() override;
  views::BubbleDialogDelegateView* GetPermissionPromptBubbleForTest() override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

  // BubbleOwnerDelegate:
  bool IsBubbleShowing() const override;

 private:
  // PermissionChip:
  void Collapse(bool allow_restart) override;

  void RecordChipButtonPressed();

  Browser* browser_ = nullptr;

  // The time when the chip was displayed.
  base::TimeTicks chip_shown_time_;

  views::BubbleDialogDelegateView* prompt_bubble_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_REQUEST_CHIP_H_
