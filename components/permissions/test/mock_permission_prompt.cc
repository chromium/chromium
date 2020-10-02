// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/test/mock_permission_prompt.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_ANDROID)
#include "ui/gfx/vector_icon_types.h"
#endif

namespace permissions {

MockPermissionPrompt::~MockPermissionPrompt() {
  if (factory_)
    factory_->HideView(this);
}

void MockPermissionPrompt::UpdateAnchorPosition() {}

PermissionPrompt::TabSwitchingBehavior
MockPermissionPrompt::GetTabSwitchingBehavior() {
#if defined(OS_ANDROID)
  return TabSwitchingBehavior::kKeepPromptAlive;
#else
  return TabSwitchingBehavior::kDestroyPromptButKeepRequestPending;
#endif
}

PermissionPromptDisposition MockPermissionPrompt::GetPromptDisposition() const {
#if defined(OS_ANDROID)
  return PermissionPromptDisposition::MODAL_DIALOG;
#else
  return PermissionPromptDisposition::ANCHORED_BUBBLE;
#endif
}

MockPermissionPrompt::MockPermissionPrompt(MockPermissionPromptFactory* factory,
                                           Delegate* delegate)
    : factory_(factory), delegate_(delegate) {
  for (const PermissionRequest* request : delegate_->Requests()) {
    // The actual prompt will call these, so test they're sane.
    EXPECT_FALSE(request->GetMessageTextFragment().empty());
#if defined(OS_ANDROID)
    // For STORAGE_ACCESS, the prompt itself calculates the message text.
    if (request->GetContentSettingsType() !=
        ContentSettingsType::STORAGE_ACCESS) {
      EXPECT_FALSE(request->GetMessageText().empty());
    }
    EXPECT_NE(0, request->GetIconId());
#else
    EXPECT_FALSE(request->GetIconId().is_empty());
#endif
  }
}

}  // namespace permissions
