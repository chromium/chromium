// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_CHIP_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_CHIP_MODEL_H_

#include <string>
#include "base/check.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_theme.h"
#include "chrome/browser/ui/views/permissions/chip_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

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

  const gfx::VectorIcon& GetIcon() {
    return should_display_blocked_icon_ ? *blocked_icon_ : *allowed_icon_;
  }

  std::u16string GetChipText() { return chip_text_; }
  std::u16string GetAccessibilityChipText() { return accessibility_chip_text_; }

  // Chip look
  PermissionPromptStyle GetPromptStyle() { return prompt_style_; }
  OmniboxChipTheme GetChipTheme() { return chip_theme_; }

  // Chip behaviour
  bool ShouldBubbleStartOpen() { return should_bubble_start_open_; }
  bool ShouldExpand() { return should_expand_; }

  // Permission state
  void SetShouldDismiss(bool flag) { should_dismiss_ = flag; }

  // Updates relevant properties of the model according to the chip's collapse
  // state if it's triggered automatically.
  void UpdateAutoCollapsePromptChipState(bool is_collapsed);

  bool ShouldDismiss() { return should_dismiss_; }

  bool IsExpandAnimationAllowed();

  bool CanDisplayConfirmation() { return chip_text_.length() > 0; }

  bool WasRequestAlreadyDisplayed() {
    DCHECK(delegate_.has_value());
    return delegate_.value()->WasCurrentRequestAlreadyDisplayed();
  }

  // Takes a user decision and updates relevant properties of the model
  void UpdateWithUserDecision(permissions::PermissionAction permission_action);

  permissions::PermissionAction GetUserDecision() { return user_decision_; }

 private:
  // Delegate holding the current request
  absl::optional<permissions::PermissionPrompt::Delegate*> delegate_;

  // Permission icons and text
  const raw_ref<const gfx::VectorIcon> allowed_icon_;
  const raw_ref<const gfx::VectorIcon> blocked_icon_;

  std::u16string chip_text_;
  std::u16string accessibility_chip_text_;

  // Chip look
  PermissionPromptStyle prompt_style_;
  OmniboxChipTheme chip_theme_;

  bool should_display_blocked_icon_ = false;

  // Chip behaviour
  bool should_bubble_start_open_ = false;
  bool should_expand_ = true;

  // Permission state
  bool should_dismiss_ = false;

  permissions::PermissionAction user_decision_ =
      permissions::PermissionAction::NUM;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_CHIP_MODEL_H_
