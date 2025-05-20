// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/permissions/permission_request_manager_test_api.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_desktop.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_data.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/content_setting_permission_resolver.h"
#include "ui/views/widget/widget.h"

namespace test {

PermissionRequestManagerTestApi::PermissionRequestManagerTestApi(
    permissions::PermissionRequestManager* manager)
    : manager_(manager) {}

PermissionRequestManagerTestApi::PermissionRequestManagerTestApi(
    Browser* browser)
    : PermissionRequestManagerTestApi(
          permissions::PermissionRequestManager::FromWebContents(
              browser->tab_strip_model()->GetActiveWebContents())) {}

void PermissionRequestManagerTestApi::AddSimpleRequest(
    content::RenderFrameHost* source_frame,
    permissions::RequestType type) {
  const bool user_gesture = true;
  manager_->AddRequest(
      source_frame,
      std::make_unique<permissions::PermissionRequest>(
          std::make_unique<permissions::PermissionRequestData>(
              std::make_unique<permissions::ContentSettingPermissionResolver>(
                  permissions::RequestTypeToContentSettingsType(type).value()),
              /*user_gesture=*/user_gesture, permission_request_origin_),
          base::DoNothing()));
}

void PermissionRequestManagerTestApi::SetOrigin(
    const GURL& permission_request_origin) {
  permission_request_origin_ = permission_request_origin;
}

views::Widget* PermissionRequestManagerTestApi::GetPromptWindow() {
  PermissionPromptDesktop* prompt =
      static_cast<PermissionPromptDesktop*>(manager_->view_.get());
  return prompt ? prompt->GetPromptBubbleWidgetForTesting() : nullptr;
}

void PermissionRequestManagerTestApi::SimulateWebContentsDestroyed() {
  manager_->WebContentsDestroyed();
}

}  // namespace test
