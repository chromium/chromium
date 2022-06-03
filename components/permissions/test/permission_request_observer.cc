// Copyright 2020 The Chromium Authors. All rights reserved.
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

void PermissionRequestObserver::OnBubbleAdded() {
  request_shown_ = true;
  loop_.Quit();
}

}  // namespace permissions
