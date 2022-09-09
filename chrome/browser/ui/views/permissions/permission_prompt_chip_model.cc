// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_chip_model.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"

namespace {
const gfx::VectorIcon& GetBlockedPermissionIconId(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);
  auto requests = delegate->Requests();
  if (requests.size() == 1)
    return requests[0]->GetBlockedIconForChip();

  // When we have two requests, it must be microphone & camera. Then we need to
  // use the icon from the camera request.
  return permissions::RequestType::kCameraStream == requests[0]->request_type()
             ? requests[0]->GetBlockedIconForChip()
             : requests[1]->GetBlockedIconForChip();
}

const gfx::VectorIcon& GetPermissionIconId(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);
  auto requests = delegate->Requests();
  if (requests.size() == 1)
    return requests[0]->GetIconForChip();

  // When we have two requests, it must be microphone & camera. Then we need to
  // use the icon from the camera request.
  return permissions::RequestType::kCameraStream == requests[0]->request_type()
             ? requests[0]->GetIconForChip()
             : requests[1]->GetIconForChip();
}

std::u16string GetQuietPermissionMessage(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);
  DCHECK(delegate->Requests()[0]->GetQuietChipText().has_value());

  return delegate->Requests()[0]->GetQuietChipText().value();
}

std::u16string GetLoudPermissionMessage(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);

  auto requests = delegate->Requests();

  return requests.size() == 1
             ? requests[0]->GetRequestChipText().value()
             : l10n_util::GetStringUTF16(
                   IDS_MEDIA_CAPTURE_VIDEO_AND_AUDIO_PERMISSION_CHIP);
}

bool ShouldPermissionBubbleExpand(
    permissions::PermissionPrompt::Delegate* delegate,
    PermissionPromptStyle prompt_style) {
  DCHECK(delegate);
  if (PermissionPromptStyle::kQuietChip == prompt_style) {
    return !permissions::PermissionUiSelector::ShouldSuppressAnimation(
        delegate->ReasonForUsingQuietUi());
  }

  return true;
}

}  // namespace

PermissionPromptChipModel::PermissionPromptChipModel(
    permissions::PermissionPrompt::Delegate* delegate)
    : delegate_(delegate),
      allowed_icon_(GetPermissionIconId(delegate)),
      blocked_icon_(GetBlockedPermissionIconId(delegate)) {
  DCHECK(delegate_);

  if (delegate_.value()->ShouldCurrentRequestUseQuietUI()) {
    prompt_style_ = PermissionPromptStyle::kQuietChip;
    should_bubble_start_open_ = false;
    should_expand_ =
        ShouldPermissionBubbleExpand(delegate_.value(), prompt_style_) &&
        (should_bubble_start_open_ ||
         (!delegate_.value()->WasCurrentRequestAlreadyDisplayed()));

    permission_message_ = GetQuietPermissionMessage(delegate_.value());
    chip_theme_ = OmniboxChipTheme::kLowVisibility;
  } else {
    prompt_style_ = PermissionPromptStyle::kChip;
    should_bubble_start_open_ =
        permissions::PermissionUtil::ShouldPermissionBubbleStartOpen(
            delegate_.value());

    should_expand_ = true;

    permission_message_ = GetLoudPermissionMessage(delegate_.value());
    chip_theme_ = OmniboxChipTheme::kNormalVisibility;
  }
}
