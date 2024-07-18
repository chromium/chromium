// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_notifications_mac.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/content_settings/browser/page_specific_content_settings.h"

PermissionPromptNotificationsMac::PermissionPromptNotificationsMac(
    content::WebContents* web_contents,
    Delegate* delegate)
    : app_id_(*web_app::WebAppTabHelper::GetAppId(web_contents)),
      delegate_(delegate) {
  CHECK_EQ(delegate->Requests().size(), 1u);
  CHECK_EQ(delegate->Requests()[0]->request_type(),
           permissions::RequestType::kNotifications);
  CHECK(!delegate->WasCurrentRequestAlreadyDisplayed());

  // Asynchronously kick of the permission request, to avoid any re-entrency
  // issues if the request were to fail synchronously.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PermissionPromptNotificationsMac::ShowPrompt,
                                weak_factory_.GetWeakPtr()));
}

PermissionPromptNotificationsMac::~PermissionPromptNotificationsMac() = default;

// static
bool PermissionPromptNotificationsMac::CanHandleRequest(
    content::WebContents* web_contents,
    Delegate* delegate) {
  if (delegate->Requests().size() != 1 ||
      delegate->Requests()[0]->request_type() !=
          permissions::RequestType::kNotifications) {
    return false;
  }
  return web_app::WebAppTabHelper::GetAppIdForNotificationAttribution(
             web_contents)
      .has_value();
}

bool PermissionPromptNotificationsMac::UpdateAnchor() {
  return true;
}

permissions::PermissionPrompt::TabSwitchingBehavior
PermissionPromptNotificationsMac::GetTabSwitchingBehavior() {
  return permissions::PermissionPrompt::TabSwitchingBehavior::kKeepPromptAlive;
}

permissions::PermissionPromptDisposition
PermissionPromptNotificationsMac::GetPromptDisposition() const {
  return permissions::PermissionPromptDisposition::MAC_OS_PROMPT;
}

bool PermissionPromptNotificationsMac::IsAskPrompt() const {
  return true;
}

std::optional<gfx::Rect>
PermissionPromptNotificationsMac::GetViewBoundsInScreen() const {
  return std::nullopt;
}

bool PermissionPromptNotificationsMac::ShouldFinalizeRequestAfterDecided()
    const {
  return true;
}

std::vector<permissions::ElementAnchoredBubbleVariant>
PermissionPromptNotificationsMac::GetPromptVariants() const {
  return {};
}

std::optional<permissions::feature_params::PermissionElementPromptPosition>
PermissionPromptNotificationsMac::GetPromptPosition() const {
  return std::nullopt;
}

void PermissionPromptNotificationsMac::ShowPrompt() {
  apps::AppShimManager::Get()->ShowNotificationPermissionRequest(
      app_id_,
      base::BindOnce(&PermissionPromptNotificationsMac::OnPermissionResult,
                     weak_factory_.GetWeakPtr()));
}

void PermissionPromptNotificationsMac::OnPermissionResult(
    mac_notifications::mojom::RequestPermissionResult result) {
  using RequestPermissionResult =
      mac_notifications::mojom::RequestPermissionResult;
  switch (result) {
    case RequestPermissionResult::kPermissionGranted:
      delegate_->Accept();
      return;
    case RequestPermissionResult::kPermissionPreviouslyDenied: {
      content::RenderFrameHost* rfh =
          delegate_->GetAssociatedWebContents()->GetPrimaryMainFrame();
      content_settings::PageSpecificContentSettings::GetForFrame(rfh)
          ->SetNotificationsWasDeniedBecauseOfSystemPermission();
      // TODO(https://crbug.com/328105508): Consider adding a new result type
      // for this rather than re-using ignore.
      delegate_->Ignore();
      break;
    }
    case RequestPermissionResult::kPermissionDenied:
      delegate_->Deny();
      return;
    case RequestPermissionResult::kPermissionPreviouslyGranted:
    case RequestPermissionResult::kRequestFailed:
      // PermissionPromptFactory only creates this class the first time a
      // particular request is shown. As such, calling RecreateView here will
      // ensure we fall back to a regular Chrome permission prompt.
      delegate_->RecreateView();
      break;
  }
}
