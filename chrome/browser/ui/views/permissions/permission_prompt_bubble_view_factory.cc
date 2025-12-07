// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_view_factory.h"

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_one_origin_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_two_origins_view.h"
#include "components/permissions/permission_request.h"

raw_ptr<PermissionPromptBubbleBaseView> CreatePermissionPromptBubbleView(
    Browser* browser,
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
    PermissionPromptStyle prompt_style) {
  if (delegate->Requests()[0]->ShouldUseTwoOriginPrompt()) {
    return new PermissionPromptBubbleTwoOriginsView(
        browser, delegate, prompt_style);
  } else {
    return new PermissionPromptBubbleOneOriginView(
        browser, delegate, prompt_style);
  }
}
