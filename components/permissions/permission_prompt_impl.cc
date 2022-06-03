// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_prompt.h"
#include "components/permissions/permissions_client.h"

namespace permissions {

// TODO(crbug.com/1025609): Move the desktop permission prompt implementations
// into //components/permissions.
std::unique_ptr<PermissionPrompt> PermissionPrompt::Create(
    content::WebContents* web_contents,
    Delegate* delegate) {
  return PermissionsClient::Get()->CreatePrompt(web_contents, delegate);
}

}  // namespace permissions
