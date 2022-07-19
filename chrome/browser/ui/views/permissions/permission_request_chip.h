// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_REQUEST_CHIP_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_REQUEST_CHIP_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/permissions/permission_chip_delegate.h"

namespace views {
class View;
}

class Browser;
class PermissionPromptBubbleView;
enum class OmniboxChipTheme;

// An implementation of PermissionChipDelegate that is used in the
// PermissionChip and provides data for a visual representation of a permission
// prompt in the form of a normal (loud) permission chip.
class PermissionRequestChip : public PermissionChipDelegate {
 public:
  // If `should_bubble_start_open` is true, a permission prompt bubble will be
  // displayed automatically after PermissionRequestChip is created.
  // `should_bubble_start_open` is evaluated based on
  // `PermissionChipGestureSensitive` and `PermissionChipRequestTypeSensitive`
  // experiments.
  PermissionRequestChip(Browser* browser,
                        permissions::PermissionPrompt::Delegate* delegate,
                        bool should_bubble_start_open);
  PermissionRequestChip(const PermissionRequestChip& chip) = delete;
  PermissionRequestChip& operator=(const PermissionRequestChip& chip) = delete;
  ~PermissionRequestChip() override;

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
  void OnPromptBubbleDismissed();

  raw_ptr<Browser> browser_ = nullptr;
  raw_ptr<PermissionPromptBubbleView> prompt_bubble_ = nullptr;
  raw_ptr<permissions::PermissionPrompt::Delegate> delegate_ = nullptr;

  // The time when the chip was displayed.
  base::TimeTicks chip_shown_time_;
  bool should_bubble_start_open_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_REQUEST_CHIP_H_
