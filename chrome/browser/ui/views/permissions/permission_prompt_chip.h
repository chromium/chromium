// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_CHIP_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_CHIP_H_

#include "base/check_is_test.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_desktop.h"
#include "components/permissions/permission_request_manager.h"
#include "ui/views/widget/widget_observer.h"

class Browser;

namespace content {
class WebContents;
}

// This class represents the UI handling of a permission request that should use
// the chip UI. Objects of this class:
// - use the ChipController to modify a chip displayed in the location
//   bar.
// - should be created once a user should be prompted a permission request using
//   the chip UI.
// - should be destroyed once the user has made a decision.
class PermissionPromptChip : public PermissionPromptDesktop {
 public:
  PermissionPromptChip(Browser* browser,
                       content::WebContents* web_contents,
                       Delegate* delegate);
  ~PermissionPromptChip() override;
  PermissionPromptChip(const PermissionPromptChip&) = delete;
  PermissionPromptChip& operator=(const PermissionPromptChip&) = delete;

  // PermissionPrompt:
  bool UpdateAnchor() override;
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override;
  std::optional<gfx::Rect> GetViewBoundsInScreen() const override;

  // PermissionPromptDesktop:
  views::Widget* GetPromptBubbleWidgetForTesting() override;

  ChipController* get_chip_controller_for_testing() {
    CHECK_IS_TEST();
    return chip_controller_;
  }

 private:
  void PreemptivelyResolvePermissionRequest(content::WebContents* web_contents,
                                            Delegate* delegate);
  // The controller handling the chip view
  raw_ptr<ChipController> chip_controller_;

  // Delegate representing a permission request
  raw_ptr<permissions::PermissionPrompt::Delegate> delegate_;

  base::WeakPtrFactory<PermissionPromptChip> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_CHIP_H_
