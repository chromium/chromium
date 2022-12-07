// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/test/permission_request_observer.h"

namespace permissions {

PermissionRequestObserver::PermissionRequestObserver(
    content::WebContents* web_contents) {
  observation_.Observe(PermissionRequestManager::FromWebContents(web_contents));
}

PermissionRequestObserver::~PermissionRequestObserver() = default;

void PermissionRequestObserver::Wait() {
  loop_.Run();
}

void PermissionRequestObserver::OnPromptAdded() {
  request_shown_ = true;
  loop_.Quit();
}

void PermissionRequestObserver::OnRequestsFinalized() {
  loop_.Quit();
}

void PermissionRequestObserver::OnPromptRecreateViewFailed() {
  is_view_recreate_failed_ = true;
  loop_.Quit();
}

void PermissionRequestObserver::OnPromptCreationFailedHiddenTab() {
  is_prompt_show_failed_hidden_tab_ = true;
  loop_.Quit();
}

void PermissionRequestObserver::OnPermissionRequestManagerDestructed() {
  observation_.Reset();
}

}  // namespace permissions
