// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_chip_model.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_theme.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
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
  auto quiet_request_text = delegate->Requests()[0]->GetRequestChipText(
      permissions::PermissionRequest::QUIET_REQUEST);
  return quiet_request_text.value_or(u"");
}

std::u16string GetLoudPermissionMessage(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);

  auto requests = delegate->Requests();

  return requests.size() == 1
             ? requests[0]
                   ->GetRequestChipText(
                       permissions::PermissionRequest::LOUD_REQUEST)
                   .value()
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

    chip_text_ = GetQuietPermissionMessage(delegate_.value());
    chip_theme_ = OmniboxChipTheme::kLowVisibility;
  } else {
    prompt_style_ = PermissionPromptStyle::kChip;
    should_bubble_start_open_ = true;

    should_expand_ = true;

    chip_text_ = GetLoudPermissionMessage(delegate_.value());
    chip_theme_ = OmniboxChipTheme::kNormalVisibility;
  }
  accessibility_chip_text_ = l10n_util::GetStringUTF16(
      IDS_PERMISSIONS_REQUESTED_SCREENREADER_ANNOUNCEMENT);
}

void PermissionPromptChipModel::UpdateAutoCollapsePromptChipState(
    bool is_collapsed) {
  should_display_blocked_icon_ = is_collapsed;
  chip_theme_ = OmniboxChipTheme::kLowVisibility;
}

bool PermissionPromptChipModel::IsExpandAnimationAllowed() {
  return ShouldExpand() &&
         (ShouldBubbleStartOpen() || !WasRequestAlreadyDisplayed());
}

void PermissionPromptChipModel::UpdateWithUserDecision(
    permissions::PermissionAction user_decision) {
  DCHECK(delegate_.has_value());

  permissions::PermissionRequest::ChipTextType chip_text_type;
  permissions::PermissionRequest::ChipTextType accessibility_text_type;
  int cam_mic_combo_accessibility_text_id;

  switch (user_decision) {
    case permissions::PermissionAction::GRANTED:
      should_display_blocked_icon_ = false;
      chip_theme_ = OmniboxChipTheme::kNormalVisibility;
      chip_text_type =
          permissions::PermissionRequest::ChipTextType::ALLOW_CONFIRMATION;
      accessibility_text_type = permissions::PermissionRequest::ChipTextType::
          ACCESSIBILITY_ALLOWED_CONFIRMATION;
      cam_mic_combo_accessibility_text_id =
          IDS_PERMISSIONS_CAMERA_AND_MICROPHONE_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT;
      break;
    case permissions::PermissionAction::GRANTED_ONCE:
      should_display_blocked_icon_ = false;
      chip_theme_ = OmniboxChipTheme::kNormalVisibility;
      chip_text_type =
          permissions::PermissionRequest::ChipTextType::ALLOW_ONCE_CONFIRMATION;
      accessibility_text_type = permissions::PermissionRequest::ChipTextType::
          ACCESSIBILITY_ALLOWED_ONCE_CONFIRMATION;
      cam_mic_combo_accessibility_text_id =
          IDS_PERMISSIONS_CAMERA_AND_MICROPHONE_ALLOWED_ONCE_CONFIRMATION_SCREENREADER_ANNOUNCEMENT;
      break;
    case permissions::PermissionAction::DENIED:
    case permissions::PermissionAction::DISMISSED:
    case permissions::PermissionAction::IGNORED:
    case permissions::PermissionAction::REVOKED:
      should_display_blocked_icon_ = true;
      chip_theme_ = OmniboxChipTheme::kLowVisibility;
      chip_text_type =
          permissions::PermissionRequest::ChipTextType::BLOCKED_CONFIRMATION;
      accessibility_text_type = permissions::PermissionRequest::ChipTextType::
          ACCESSIBILITY_BLOCKED_CONFIRMATION;
      cam_mic_combo_accessibility_text_id =
          IDS_PERMISSIONS_CAMERA_AND_MICROPHONE_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT;
      break;
    case permissions::PermissionAction::NUM:
      NOTREACHED_NORETURN();
  }

  chip_text_ = delegate_.value()
                   ->Requests()[0]
                   ->GetRequestChipText(chip_text_type)
                   .value_or(u"");
  if (delegate_.value()->Requests().size() == 1) {
    accessibility_chip_text_ = delegate_.value()
                                   ->Requests()[0]
                                   ->GetRequestChipText(accessibility_text_type)
                                   .value_or(u"");
  } else {
    accessibility_chip_text_ =
        l10n_util::GetStringUTF16(cam_mic_combo_accessibility_text_id);
  }
}
