// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/permissions/permission_request_manager_test_api.h"

#include <memory>
#include "base/functional/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_desktop.h"
#include "components/permissions/permission_request.h"
#include "ui/views/widget/widget.h"

namespace test {
namespace {

// Wraps a PermissionRequest so that it can pass a closure to itself to the
// PermissionRequest constructor. Without this wrapper, there's no way to
// handle all destruction paths.
class TestPermissionRequestOwner {
 public:
  explicit TestPermissionRequestOwner(permissions::RequestType type,
                                      GURL& origin) {
    const bool user_gesture = true;
    auto decided = [](ContentSetting, bool, bool) {};
    request_ = std::make_unique<permissions::PermissionRequest>(
        origin, type, user_gesture, base::BindRepeating(decided),
        base::BindOnce(&TestPermissionRequestOwner::DeleteThis,
                       base::Unretained(this)));
  }

  TestPermissionRequestOwner(const TestPermissionRequestOwner&) = delete;
  TestPermissionRequestOwner& operator=(const TestPermissionRequestOwner&) =
      delete;

  permissions::PermissionRequest* request() { return request_.get(); }

 private:
  void DeleteThis() { delete this; }

  std::unique_ptr<permissions::PermissionRequest> request_;
};

}  // namespace

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
  TestPermissionRequestOwner* request_owner =
      new TestPermissionRequestOwner(type, permission_request_origin_);
  manager_->AddRequest(source_frame, request_owner->request());
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
