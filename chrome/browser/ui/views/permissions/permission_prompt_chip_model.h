// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_CHIP_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_CHIP_MODEL_H_

#include <string>
#include "base/check.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_theme.h"
#include "chrome/browser/ui/views/permissions/chip_controller.h"
#include "components/permissions/features.h"

class PermissionPromptChipModel {
 public:
  explicit PermissionPromptChipModel(
      permissions::PermissionPrompt::Delegate* delegate);
  ~PermissionPromptChipModel() = default;
  PermissionPromptChipModel(const PermissionPromptChipModel&) = delete;
  PermissionPromptChipModel& operator=(const PermissionPromptChipModel&) =
      delete;

  void ResetDelegate() { delegate_.reset(); }

  // The delegate representing the permission request
  absl::optional<permissions::PermissionPrompt::Delegate*> GetDelegate() {
    return delegate_;
  }

  // Permission icons and text
  const gfx::VectorIcon& GetAllowedIcon() { return allowed_icon_; }
  const gfx::VectorIcon& GetBlockedIcon() { return blocked_icon_; }
  std::u16string GetPermissionMessage() { return permission_message_; }

  // Chip look
  PermissionPromptStyle GetPromptStyle() { return prompt_style_; }
  OmniboxChipTheme GetChipTheme() { return chip_theme_; }

  // Chip behaviour
  bool ShouldBubbleStartOpen() { return should_bubble_start_open_; }
  bool ShouldExpand() { return should_expand_; }

  // Permission state
  void SetShouldDismiss(bool flag) { should_dismiss_ = flag; }
  bool ShouldDismiss() { return should_dismiss_; }
  bool WasRequestAlreadyDisplayed() {
    DCHECK(delegate_.has_value());
    return delegate_.value()->WasCurrentRequestAlreadyDisplayed();
  }

 private:
  // Delegate representing a permission request
  absl::optional<permissions::PermissionPrompt::Delegate*> delegate_;

  // Permission icons and text
  const gfx::VectorIcon& allowed_icon_;
  const gfx::VectorIcon& blocked_icon_;
  std::u16string permission_message_;

  // Chip look
  PermissionPromptStyle prompt_style_;
  OmniboxChipTheme chip_theme_;

  // Chip behaviour
  bool should_bubble_start_open_ = false;
  bool should_expand_ = true;

  // Permission state
  bool should_dismiss_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_CHIP_MODEL_H_
