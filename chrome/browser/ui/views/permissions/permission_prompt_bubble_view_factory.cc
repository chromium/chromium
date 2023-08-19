// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_view_factory.h"

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_one_origin_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_two_origins_view.h"

raw_ptr<PermissionPromptBubbleBaseView> CreatePermissionPromptBubbleView(
    Browser* browser,
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
    base::TimeTicks permission_requested_time,
    PermissionPromptStyle prompt_style) {
  if (delegate->Requests()[0]->ShouldUseTwoOriginPrompt()) {
    return new PermissionPromptBubbleTwoOriginsView(
        browser, delegate, permission_requested_time, prompt_style);
  } else {
    return new PermissionPromptBubbleOneOriginView(
        browser, delegate, permission_requested_time, prompt_style);
  }
}
