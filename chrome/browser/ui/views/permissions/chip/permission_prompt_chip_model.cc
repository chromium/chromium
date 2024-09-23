// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/chip/permission_prompt_chip_model.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
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
  DCHECK(delegate->ReasonForUsingQuietUi());
  auto chip_text_type = permissions::PermissionRequest::QUIET_REQUEST;
  const auto quiet_ui_reason = delegate->ReasonForUsingQuietUi();
  if (quiet_ui_reason == permissions::PermissionRequestManager::QuietUiReason::
                             kServicePredictedVeryUnlikelyGrant ||
      quiet_ui_reason == permissions::PermissionRequestManager::QuietUiReason::
                             kOnDevicePredictedVeryUnlikelyGrant) {
    chip_text_type = base::FeatureList::IsEnabled(
                         permissions::features::kCpssQuietChipTextUpdate)
                         ? permissions::PermissionRequest::LOUD_REQUEST
                         : permissions::PermissionRequest::QUIET_REQUEST;
  }
  auto quiet_request_text =
      delegate->Requests()[0]->GetRequestChipText(chip_text_type);
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
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate)
    : delegate_(delegate),
      allowed_icon_(GetPermissionIconId(delegate.get())),
      blocked_icon_(GetBlockedPermissionIconId(delegate.get())) {
  DCHECK(delegate_);

  if (delegate_->ShouldCurrentRequestUseQuietUI()) {
    DCHECK(delegate_->ReasonForUsingQuietUi());
    prompt_style_ = PermissionPromptStyle::kQuietChip;
    should_bubble_start_open_ = false;
    const auto quiet_ui_reason = delegate_->ReasonForUsingQuietUi();
    if (quiet_ui_reason ==
            permissions::PermissionRequestManager::QuietUiReason::
                kOnDevicePredictedVeryUnlikelyGrant ||
        quiet_ui_reason ==
            permissions::PermissionRequestManager::QuietUiReason::
                kServicePredictedVeryUnlikelyGrant) {
      should_display_blocked_icon_ = !base::FeatureList::IsEnabled(
          permissions::features::kCpssQuietChipTextUpdate);
    } else {
      should_display_blocked_icon_ = true;
    }
    should_expand_ =
        ShouldPermissionBubbleExpand(delegate_.get(), prompt_style_) &&
        (should_bubble_start_open_ ||
         (!delegate_->WasCurrentRequestAlreadyDisplayed()));

    chip_text_ = GetQuietPermissionMessage(delegate_.get());
    chip_theme_ = PermissionChipTheme::kLowVisibility;
  } else {
    prompt_style_ = PermissionPromptStyle::kChip;
    should_bubble_start_open_ = true;
    should_display_blocked_icon_ = false;
    should_expand_ = true;

    chip_text_ = GetLoudPermissionMessage(delegate_.get());
    chip_theme_ = PermissionChipTheme::kNormalVisibility;
  }
  accessibility_chip_text_ = l10n_util::GetStringUTF16(
      IDS_PERMISSIONS_REQUESTED_SCREENREADER_ANNOUNCEMENT);

  if (delegate->Requests().size() > 1) {
    // Multiple requests allowed for `MEDIASTREAM_CAMERA` and `MEDIASTREAM_MIC`.
    // In that case we show only a camera indicator.
    content_settings_type_ = ContentSettingsType::MEDIASTREAM_CAMERA;
  } else {
    content_settings_type_ = delegate->Requests()[0]->GetContentSettingsType();
  }
}

PermissionPromptChipModel::~PermissionPromptChipModel() = default;

void PermissionPromptChipModel::UpdateAutoCollapsePromptChipState(
    bool is_collapsed) {
  should_display_blocked_icon_ = is_collapsed;
  chip_theme_ = PermissionChipTheme::kLowVisibility;
}

bool PermissionPromptChipModel::IsExpandAnimationAllowed() {
  return ShouldExpand() && (ShouldBubbleStartOpen() ||
                            !delegate_->WasCurrentRequestAlreadyDisplayed());
}

void PermissionPromptChipModel::UpdateWithUserDecision(
    permissions::PermissionAction user_decision) {
  permissions::PermissionRequest::ChipTextType chip_text_type;
  permissions::PermissionRequest::ChipTextType accessibility_text_type;
  user_decision_ = user_decision;

  int cam_mic_combo_accessibility_text_id;
  switch (user_decision_) {
    case permissions::PermissionAction::GRANTED:
      should_display_blocked_icon_ = false;
      chip_theme_ = PermissionChipTheme::kNormalVisibility;
      chip_text_type =
          permissions::PermissionRequest::ChipTextType::ALLOW_CONFIRMATION;
      accessibility_text_type = permissions::PermissionRequest::ChipTextType::
          ACCESSIBILITY_ALLOWED_CONFIRMATION;
      cam_mic_combo_accessibility_text_id =
          IDS_PERMISSIONS_CAMERA_AND_MICROPHONE_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT;
      break;
    case permissions::PermissionAction::GRANTED_ONCE:
      should_display_blocked_icon_ = false;
      chip_theme_ = PermissionChipTheme::kNormalVisibility;
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
      chip_theme_ = PermissionChipTheme::kLowVisibility;
      chip_text_type =
          permissions::PermissionRequest::ChipTextType::BLOCKED_CONFIRMATION;
      accessibility_text_type = permissions::PermissionRequest::ChipTextType::
          ACCESSIBILITY_BLOCKED_CONFIRMATION;
      cam_mic_combo_accessibility_text_id =
          IDS_PERMISSIONS_CAMERA_AND_MICROPHONE_NOT_ALLOWED_CONFIRMATION_SCREENREADER_ANNOUNCEMENT;
      break;
    case permissions::PermissionAction::NUM:
      NOTREACHED();
  }

  chip_text_ = delegate_->Requests()[0]
                   ->GetRequestChipText(chip_text_type)
                   .value_or(u"");
  if (delegate_->Requests().size() == 1) {
    accessibility_chip_text_ = delegate_->Requests()[0]
                                   ->GetRequestChipText(accessibility_text_type)
                                   .value_or(u"");
  } else {
    accessibility_chip_text_ =
        l10n_util::GetStringUTF16(cam_mic_combo_accessibility_text_id);
  }
}
