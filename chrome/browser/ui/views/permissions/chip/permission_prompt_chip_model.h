// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_PROMPT_CHIP_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_PROMPT_CHIP_MODEL_H_

#include <string>

#include "base/check.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_theme.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

class PermissionPromptChipModel {
 public:
  explicit PermissionPromptChipModel(
      base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate);
  ~PermissionPromptChipModel();
  PermissionPromptChipModel(const PermissionPromptChipModel&) = delete;
  PermissionPromptChipModel& operator=(const PermissionPromptChipModel&) =
      delete;

  // The delegate representing the permission request.
  const base::WeakPtr<permissions::PermissionPrompt::Delegate>& GetDelegate() {
    return delegate_;
  }

  const gfx::VectorIcon& GetIcon() {
    return should_display_blocked_icon_ ? *blocked_icon_ : *allowed_icon_;
  }

  std::u16string GetChipText() { return chip_text_; }
  std::u16string GetAccessibilityChipText() { return accessibility_chip_text_; }

  PermissionPromptStyle GetPromptStyle() { return prompt_style_; }
  PermissionChipTheme GetChipTheme() { return chip_theme_; }

  bool ShouldDisplayBlockedIcon() { return should_display_blocked_icon_; }
  bool ShouldBubbleStartOpen() { return should_bubble_start_open_; }
  bool ShouldExpand() { return should_expand_; }

  // Updates relevant properties of the model according to the chip's collapse
  // state if it's triggered automatically.
  void UpdateAutoCollapsePromptChipState(bool is_collapsed);

  bool IsExpandAnimationAllowed();

  bool CanDisplayConfirmation() { return chip_text_.length() > 0; }

  // Takes a user decision and updates relevant properties of the model.
  void UpdateWithUserDecision(permissions::PermissionAction permission_action);

  permissions::PermissionAction GetUserDecision() { return user_decision_; }

  ContentSettingsType content_settings_type() const {
    return content_settings_type_;
  }

 private:
  // Delegate holding the current request.
  base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate_;

  // Permission icons and text.
  const raw_ref<const gfx::VectorIcon> allowed_icon_;
  const raw_ref<const gfx::VectorIcon> blocked_icon_;

  std::u16string chip_text_;
  std::u16string accessibility_chip_text_;

  PermissionPromptStyle prompt_style_;
  PermissionChipTheme chip_theme_;

  bool should_display_blocked_icon_ = false;

  // A flag that indicates if a permission popup bubble should be automatically
  // opened.
  bool should_bubble_start_open_ = false;

  // A flag that indicates if a chip should start in the verbose form or as an
  // icon only.
  bool should_expand_ = true;

  ContentSettingsType content_settings_type_;

  permissions::PermissionAction user_decision_ =
      permissions::PermissionAction::NUM;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_PROMPT_CHIP_MODEL_H_
