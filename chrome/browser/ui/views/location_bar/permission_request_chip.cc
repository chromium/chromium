// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/permission_request_chip.h"

#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_bubble_view.h"
#include "chrome/browser/ui/views/permission_bubble/permission_prompt_style.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {
bool IsCameraPermission(permissions::RequestType type) {
  return type == permissions::RequestType::kCameraStream;
}
}  // namespace

PermissionRequestChip::PermissionRequestChip(Browser* browser)
    : browser_(browser) {}

PermissionRequestChip::~PermissionRequestChip() {
  if (prompt_bubble_)
    prompt_bubble_->GetWidget()->Close();
}

void PermissionRequestChip::DisplayRequest(
    permissions::PermissionPrompt::Delegate* delegate) {
  chip_shown_time_ = base::TimeTicks::Now();
  PermissionChip::DisplayRequest(delegate);
}

void PermissionRequestChip::FinalizeRequest() {
  PermissionChip::FinalizeRequest();
  already_recorded_interaction_ = false;
  if (prompt_bubble_)
    prompt_bubble_->GetWidget()->Close();
}

void PermissionRequestChip::OpenBubble() {
  // The prompt bubble is either not opened yet or already closed on
  // deactivation.
  DCHECK(!prompt_bubble_);

  PermissionPromptBubbleView* prompt_bubble = new PermissionPromptBubbleView(
      browser_, delegate(), chip_shown_time_, PermissionPromptStyle::kChip);
  prompt_bubble->Show();
  prompt_bubble->GetWidget()->AddObserver(this);
  prompt_bubble_ = prompt_bubble;

  RecordChipButtonPressed();
}

void PermissionRequestChip::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, prompt_bubble_->GetWidget());
  PermissionChip::OnWidgetDestroying(widget);
  prompt_bubble_ = nullptr;
}

views::BubbleDialogDelegateView*
PermissionRequestChip::GetPermissionPromptBubbleForTest() {
  return prompt_bubble_;
}

bool PermissionRequestChip::IsBubbleShowing() const {
  return prompt_bubble_;
}

const gfx::VectorIcon& PermissionRequestChip::GetPermissionIconId() const {
  auto requests = delegate()->Requests();
  if (requests.size() == 1)
    return permissions::GetIconId(requests[0]->GetRequestType());

  // When we have two requests, it must be microphone & camera. Then we need to
  // use the icon from the camera request.
  return IsCameraPermission(requests[0]->GetRequestType())
             ? permissions::GetIconId(requests[0]->GetRequestType())
             : permissions::GetIconId(requests[1]->GetRequestType());
}

std::u16string PermissionRequestChip::GetPermissionMessage() const {
  if (!delegate())
    return std::u16string();
  auto requests = delegate()->Requests();

  return requests.size() == 1
             ? requests[0]->GetChipText().value()
             : l10n_util::GetStringUTF16(
                   IDS_MEDIA_CAPTURE_VIDEO_AND_AUDIO_PERMISSION_CHIP);
}

void PermissionRequestChip::RecordChipButtonPressed() {
  if (!already_recorded_interaction_) {
    base::UmaHistogramLongTimes("Permissions.Chip.TimeToInteraction",
                                base::TimeTicks::Now() - chip_shown_time_);
    already_recorded_interaction_ = true;
  }
}

BEGIN_METADATA(PermissionRequestChip, views::View)
END_METADATA
