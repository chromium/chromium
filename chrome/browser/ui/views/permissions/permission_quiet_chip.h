// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_QUIET_CHIP_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_QUIET_CHIP_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/permissions/permission_chip_delegate.h"

namespace views {
class Widget;
}

class Browser;
class LocationBarView;

// A less prominent version of `PermissionRequestChip`. It is used to display a
// permission request from origins with an abusive reputation, low acceptance
// rate, or if a user manually enabled "quieter messaging" in
// chrome://settings/content/notifications.
class PermissionQuietChip : public PermissionChipDelegate {
 public:
  PermissionQuietChip(Browser* browser,
                      permissions::PermissionPrompt::Delegate* delegate,
                      bool should_expand);
  PermissionQuietChip(const PermissionQuietChip& chip) = delete;
  PermissionQuietChip& operator=(const PermissionQuietChip& chip) = delete;
  ~PermissionQuietChip() override;

 private:
  // PermissionChipDelegate:
  views::View* CreateBubble() override;
  void ShowBubble() override;
  const gfx::VectorIcon& GetIconOn() override;
  const gfx::VectorIcon& GetIconOff() override;
  std::u16string GetMessage() override;
  bool ShouldStartOpen() override;
  bool ShouldExpand() override;
  OmniboxChipTheme GetTheme() override;
  permissions::PermissionPrompt::Delegate* GetPermissionPromptDelegate()
      override;

  void RecordChipButtonPressed();
  LocationBarView* GetLocationBarView();

  raw_ptr<Browser> browser_ = nullptr;
  raw_ptr<views::Widget> bubble_widget_ = nullptr;
  raw_ptr<permissions::PermissionPrompt::Delegate> delegate_ = nullptr;

  // The time when the chip was displayed.
  base::TimeTicks chip_shown_time_;

  bool should_bubble_start_open_ = false;
  bool should_expand_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_QUIET_CHIP_H_
