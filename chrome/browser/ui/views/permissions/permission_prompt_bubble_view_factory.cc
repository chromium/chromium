// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_view_factory.h"

#include "base/check.h"
#include "chrome/browser/ui/views/permissions/ambient_login_permission_bubble_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_one_origin_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_two_origins_view.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/request_type.h"

raw_ptr<PermissionPromptBubbleBaseView> CreatePermissionPromptBubbleView(
    content::WebContents* web_contents,
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
    PermissionPromptStyle prompt_style) {
  CHECK(delegate);
  CHECK(!delegate->Requests().empty());
  if (delegate->Requests()[0]->request_type() ==
      permissions::RequestType::kAmbientLogin) {
    CHECK_EQ(delegate->Requests().size(), 1u);
    return new AmbientLoginPermissionBubbleView(web_contents, delegate,
                                                prompt_style);
  }
  if (delegate->Requests()[0]->ShouldUseTwoOriginPrompt()) {
    return new PermissionPromptBubbleTwoOriginsView(web_contents, delegate,
                                                    prompt_style);
  } else {
    return new PermissionPromptBubbleOneOriginView(web_contents, delegate,
                                                   prompt_style);
  }
}
