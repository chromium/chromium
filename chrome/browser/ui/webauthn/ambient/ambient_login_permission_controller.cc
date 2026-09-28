// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_controller.h"

#include <utility>

#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace ambient_signin {

AmbientLoginPermissionController::AmbientLoginPermissionController(
    content::Page& page)
    : content::PageUserData<AmbientLoginPermissionController>(page) {}

AmbientLoginPermissionController::~AmbientLoginPermissionController() = default;

void AmbientLoginPermissionController::RequestPermission(
    content::RenderFrameHost* requesting_frame,
    std::unique_ptr<AmbientLoginPermissionRequest> request) {
  if (!page().IsPrimary()) {
    request->Cancelled();
    return;
  }

  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&page().GetMainDocument());
  auto* manager =
      web_contents
          ? permissions::PermissionRequestManager::FromWebContents(web_contents)
          : nullptr;
  if (!manager || state_ != State::kIdle) {
    request->Cancelled();
    return;
  }

  state_ = State::kRequestAdded;

  request->set_request_finished_callback(
      base::BindOnce(&AmbientLoginPermissionController::OnRequestFinished,
                     weak_ptr_factory_.GetWeakPtr()));

  manager->AddRequest(requesting_frame, std::move(request));
}

void AmbientLoginPermissionController::SetFinishedNotificationForTesting(
    base::OnceClosure finished_closure) {
  finished_closure_ = std::move(finished_closure);
}

void AmbientLoginPermissionController::OnRequestFinished() {
  state_ = State::kIdle;
  if (finished_closure_) {
    std::move(finished_closure_).Run();
  }
}

base::WeakPtr<AmbientLoginPermissionController>
AmbientLoginPermissionController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

PAGE_USER_DATA_KEY_IMPL(AmbientLoginPermissionController);

}  // namespace ambient_signin
