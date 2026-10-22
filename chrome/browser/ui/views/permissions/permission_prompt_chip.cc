// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_chip.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_prompt_chip_model.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_observer.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

PermissionPromptChip::PermissionPromptChip(content::WebContents* web_contents,
                                           Delegate* delegate)
    : PermissionPromptDesktop(web_contents, delegate) {
  CHECK(delegate, base::NotFatalUntil::M162);
  LocationBar* lb = GetLocationBar();

  // Before showing a chip make sure the LocationBar is in a valid state. That
  // fixes a bug when a chip overlays the padlock icon.
  lb->InvalidateLayout();

  if (delegate->ShouldCurrentRequestUseQuietUI()) {
    delegate->PreIgnoreQuietPrompt();
  }

  chip_controller_ = lb->GetChipController();
  // Observe before showing the prompt, which may open the bubble synchronously.
  chip_controller_observation_.Observe(chip_controller_);
  chip_controller_->ShowPermissionPrompt(delegate->GetWeakPtr());
}

PermissionPromptChip::~PermissionPromptChip() {
  if (chip_controller_) {
    chip_controller_->ResetPermissionRequestChip();
  }
}

bool PermissionPromptChip::UpdateAnchor() {
  if (UpdateBrowser()) {
    // A ChipController instance is owned by a LocationBarView, which in turn
    // is owned by the browser instance. Hence we have to recreate the view.
    return false;
  }

  LocationBar* lb = GetLocationBar();

  if (!lb || !lb->IsInitialized()) {
    return false;  // view should be recreated
  }

  const bool is_location_bar_drawn = lb->IsDrawn() && !lb->IsFullscreen();
  if (chip_controller_->IsPermissionPromptChipVisible() &&
      !is_location_bar_drawn) {
    chip_controller_->ResetPermissionPromptChip();
    if (delegate()) {
      return false;
    }
  }
  return true;
}

permissions::PermissionPromptDisposition
PermissionPromptChip::GetPromptDisposition() const {
  if (delegate()->ShouldCurrentRequestUseQuietUI()) {
    return permissions::PermissionUiSelector::ShouldSuppressAnimation(
               delegate()->ReasonForUsingQuietUi())
               ? permissions::PermissionPromptDisposition::
                     LOCATION_BAR_LEFT_QUIET_ABUSIVE_CHIP
               : permissions::PermissionPromptDisposition::
                     LOCATION_BAR_LEFT_QUIET_CHIP;
  }

  return PermissionPromptChipModel::ShouldLoudChipBubbleStartOpen(
             delegate()->Requests())
             ? permissions::PermissionPromptDisposition::
                   LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE
             : permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP;
}

std::optional<gfx::Rect> PermissionPromptChip::GetViewBoundsInScreen() const {
  return chip_controller_->IsPermissionPromptChipVisible() &&
                 chip_controller_->IsBubbleShowing()
             ? std::make_optional<gfx::Rect>(chip_controller_->GetBubbleWidget()
                                                 ->GetWindowBoundsInScreen())
             : std::nullopt;
}

views::Widget* PermissionPromptChip::GetPromptBubbleWidgetForTesting() {
  CHECK_IS_TEST();
  LocationBar* lb = GetLocationBar();

  return chip_controller_->IsPermissionPromptChipVisible() &&
                 lb->GetChipController()->IsBubbleShowing()
             ? lb->GetChipController()->GetBubbleWidget()
             : nullptr;
}

// `ChipController::Observer` callbacks invoked when the permission prompt
// bubble anchored to the chip opens or closes. Forwarding this to
// `PermissionPromptObserver` lets WebUI consumers (e.g. composebox voice
// search) know that a `blur` event came from the permission bubble stealing
// focus rather than an outside interaction.
void PermissionPromptChip::OnPermissionPromptShown() {
  SetPromptBubbleShowing(true);
}

void PermissionPromptChip::OnPermissionPromptHidden() {
  SetPromptBubbleShowing(false);
}

void PermissionPromptChip::SetPromptBubbleShowing(bool is_showing) {
  if (is_prompt_bubble_showing_ == is_showing) {
    return;
  }
  is_prompt_bubble_showing_ = is_showing;
  if (auto* observer =
          PermissionPromptObserver::FromWebContents(web_contents())) {
    // Like `PermissionPromptBubble`, there is no minimum size to report.
    observer->NotifyPermissionPromptChanged(is_showing, gfx::Size());
  }
}
