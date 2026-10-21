// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_PROMPT_CHIP_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_PROMPT_CHIP_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_theme.h"
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

  // Returns whether a loud chip for `requests` opens its bubble along with the
  // chip. The requests share one bubble, which opens automatically unless all
  // of them start with only the chip.
  static bool ShouldLoudChipBubbleStartOpen(
      const std::vector<std::unique_ptr<permissions::PermissionRequest>>&
          requests);

  // The delegate representing the permission request.
  const base::WeakPtr<permissions::PermissionPrompt::Delegate>& GetDelegate()
      const {
    return delegate_;
  }

  const gfx::VectorIcon& GetIcon() const {
    return should_display_blocked_icon_ ? *blocked_icon_ : *allowed_icon_;
  }

  std::u16string GetChipText() const { return chip_text_; }
  std::u16string GetAccessibilityChipText() const {
    return accessibility_chip_text_;
  }

  PermissionPromptStyle GetPromptStyle() const { return prompt_style_; }
  PermissionChipTheme GetChipTheme() const { return chip_theme_; }

  bool ShouldDisplayBlockedIcon() const { return should_display_blocked_icon_; }
  bool ShouldBubbleStartOpen() const { return should_bubble_start_open_; }
  bool ShouldExpand() const { return should_expand_; }
  bool ShouldKeepChipOnBubbleClose() const {
    return should_keep_chip_on_bubble_close_;
  }

  // Updates relevant properties of the model according to the chip's collapse
  // state if it's triggered automatically.
  void UpdateAutoCollapsePromptChipState(bool is_collapsed);

  bool IsExpandAnimationAllowed() const;

  bool CanDisplayConfirmation() const { return chip_text_.length() > 0; }

  // Takes a user decision and updates relevant properties of the model.
  void UpdateWithUserDecision(permissions::PermissionAction permission_action);

  permissions::PermissionAction GetUserDecision() const {
    return user_decision_;
  }

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

  // A flag that indicates if the request stays pending, with the chip still
  // showing, when the user closes the permission popup bubble without
  // deciding.
  bool should_keep_chip_on_bubble_close_ = false;

  ContentSettingsType content_settings_type_;

  permissions::PermissionAction user_decision_ =
      permissions::PermissionAction::NUM;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_PROMPT_CHIP_MODEL_H_
