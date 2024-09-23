// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/test/mock_permission_prompt.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "ui/gfx/vector_icon_types.h"
#endif

namespace permissions {

MockPermissionPrompt::~MockPermissionPrompt() {
  if (factory_)
    factory_->HideView(this);
}

bool MockPermissionPrompt::UpdateAnchor() {
  return true;
}

PermissionPrompt::TabSwitchingBehavior
MockPermissionPrompt::GetTabSwitchingBehavior() {
#if BUILDFLAG(IS_ANDROID)
  return TabSwitchingBehavior::kKeepPromptAlive;
#else
  return TabSwitchingBehavior::kDestroyPromptButKeepRequestPending;
#endif
}

PermissionPromptDisposition MockPermissionPrompt::GetPromptDisposition() const {
#if BUILDFLAG(IS_ANDROID)
  return PermissionPromptDisposition::MODAL_DIALOG;
#else
  return PermissionPromptDisposition::ANCHORED_BUBBLE;
#endif
}

std::optional<gfx::Rect> MockPermissionPrompt::GetViewBoundsInScreen() const {
  return std::make_optional<gfx::Rect>(100, 100, 100, 100);
}

bool MockPermissionPrompt::ShouldFinalizeRequestAfterDecided() const {
  return true;
}

std::vector<permissions::ElementAnchoredBubbleVariant>
MockPermissionPrompt::GetPromptVariants() const {
  return {};
}

std::optional<feature_params::PermissionElementPromptPosition>
MockPermissionPrompt::GetPromptPosition() const {
  return std::nullopt;
}

bool MockPermissionPrompt::IsAskPrompt() const {
  return true;
}

MockPermissionPrompt::MockPermissionPrompt(MockPermissionPromptFactory* factory,
                                           Delegate* delegate)
    : factory_(factory), delegate_(delegate) {
  for (const PermissionRequest* request : delegate_->Requests()) {
    RequestType request_type = request->request_type();
    // The actual prompt will call these, so test they're sane.
#if BUILDFLAG(IS_ANDROID)
    // For kStorageAccess, the prompt itself calculates the message text.
    if (request_type != permissions::RequestType::kStorageAccess) {
      EXPECT_FALSE(
          request
              ->GetDialogAnnotatedMessageText(delegate_->GetRequestingOrigin())
              .text.empty());
    }
    EXPECT_NE(0, permissions::GetIconId(request_type));
#else
    EXPECT_FALSE(request->GetMessageTextFragment().empty());
    EXPECT_FALSE(permissions::GetIconId(request_type).is_empty());
#endif
    EXPECT_EQ(request->ShouldUseTwoOriginPrompt(),
              request_type == permissions::RequestType::kStorageAccess);
  }
}

}  // namespace permissions
