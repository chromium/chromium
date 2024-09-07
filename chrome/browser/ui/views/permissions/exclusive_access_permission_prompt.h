// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EXCLUSIVE_ACCESS_PERMISSION_PROMPT_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EXCLUSIVE_ACCESS_PERMISSION_PROMPT_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_content_scrim_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_desktop.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/request_type.h"

class Browser;
class ExclusiveAccessPermissionPromptView;

namespace content {
class WebContents;
}

namespace views {
class Widget;
}

// Controls a prompt for a set of exclusive access (keyboard/pointer lock)
// permission requests.
class ExclusiveAccessPermissionPrompt
    : public PermissionPromptDesktop,
      public EmbeddedPermissionPromptContentScrimView::Delegate {
 public:
  ExclusiveAccessPermissionPrompt(
      Browser* browser,
      content::WebContents* web_contents,
      permissions::PermissionPrompt::Delegate* delegate);
  ~ExclusiveAccessPermissionPrompt() override;
  ExclusiveAccessPermissionPrompt(const ExclusiveAccessPermissionPrompt&) =
      delete;
  ExclusiveAccessPermissionPrompt& operator=(
      const ExclusiveAccessPermissionPrompt&) = delete;

  // PermissionPromptDesktop:
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override;

  // EmbeddedPermissionPromptContentScrimView::Delegate:
  void DismissScrim() override;
  base::WeakPtr<permissions::PermissionPrompt::Delegate>
  GetPermissionPromptDelegate() const override;

  ExclusiveAccessPermissionPromptView* GetViewForTesting();

 private:
  bool ShowPrompt();
  void ClosePrompt();

  std::unique_ptr<views::Widget> content_scrim_widget_;
  views::ViewTracker prompt_view_tracker_;

  const raw_ptr<permissions::PermissionPrompt::Delegate> delegate_;

  base::WeakPtrFactory<ExclusiveAccessPermissionPrompt> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EXCLUSIVE_ACCESS_PERMISSION_PROMPT_H_
