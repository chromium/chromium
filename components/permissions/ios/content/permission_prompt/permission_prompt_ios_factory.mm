// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/ios/content/permission_prompt/permission_dialog.h"
#include "components/permissions/permission_prompt.h"
#include "content/public/browser/web_contents.h"

namespace permissions {

std::unique_ptr<PermissionPrompt> PermissionPrompt::Create(
    content::WebContents* web_contents,
    Delegate* delegate) {
  return PermissionDialog::Create(web_contents, delegate);
}

EmbeddedPermissionPromptFlowModel*
PermissionPrompt::Delegate::GetEmbeddedPromptFlowModel() const {
  return nullptr;
}

void PermissionPrompt::Delegate::CalculateCurrentVariantForEmbeddedPrompt() {}

void PermissionPrompt::Delegate::AdvanceOrFinalizeEmbeddedPromptFlow() {}

}  // namespace permissions
