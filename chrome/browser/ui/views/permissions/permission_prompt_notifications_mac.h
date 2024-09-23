// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_NOTIFICATIONS_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_NOTIFICATIONS_MAC_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "components/permissions/permission_prompt.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/weak_document_ptr.h"

// PermissionPrompt implementation that delegates the permission request to an
// app shim associated with an (installed) PWA. If showing the OS-native prompt
// in the app shim fails, this will call RecreateView() to fall back to a
// regular chrome permission prompt. Additionally, if the app shim had
// previously been granted OS-level notifications permission, we'll also fall
// back to a regular chrome prompt, rather than automatically granting
// notification permission without showing any prompt.
class PermissionPromptNotificationsMac : public permissions::PermissionPrompt {
 public:
  PermissionPromptNotificationsMac(content::WebContents* web_contents,
                                   Delegate* delegate);

  PermissionPromptNotificationsMac(const PermissionPromptNotificationsMac&) =
      delete;
  PermissionPromptNotificationsMac& operator=(
      const PermissionPromptNotificationsMac&) = delete;

  ~PermissionPromptNotificationsMac() override;

  // Returns true if the current request for `delegate` is a notification
  // permission request for a locally installed PWA.
  static bool CanHandleRequest(content::WebContents* web_contents,
                               Delegate* delegate);

  // permissions::PermissionPrompt:
  bool UpdateAnchor() override;
  TabSwitchingBehavior GetTabSwitchingBehavior() override;
  permissions::PermissionPromptDisposition GetPromptDisposition()
      const override;
  bool IsAskPrompt() const override;
  std::optional<gfx::Rect> GetViewBoundsInScreen() const override;
  bool ShouldFinalizeRequestAfterDecided() const override;
  std::vector<permissions::ElementAnchoredBubbleVariant> GetPromptVariants()
      const override;
  std::optional<permissions::feature_params::PermissionElementPromptPosition>
  GetPromptPosition() const override;

 private:
  void ShowPrompt();
  void OnPermissionResult(
      mac_notifications::mojom::RequestPermissionResult result);

  const webapps::AppId app_id_;
  const raw_ptr<permissions::PermissionPrompt::Delegate> delegate_;
  base::WeakPtrFactory<PermissionPromptNotificationsMac> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_PERMISSION_PROMPT_NOTIFICATIONS_MAC_H_
