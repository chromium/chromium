// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_chip.h"

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/permission_chip.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/web_contents.h"

PermissionPromptChip::PermissionPromptChip(Browser* browser,
                                           content::WebContents* web_contents,
                                           Delegate* delegate)
    : PermissionPromptDesktop(browser, web_contents, delegate) {
  LocationBarView* lbv = GetLocationBarView();
  if (delegate->ShouldCurrentRequestUseQuietUI()) {
    lbv->chip()->ShowQuietChip(browser, delegate);
  } else {
    lbv->chip()->ShowLoudChip(browser, delegate);
  }
}

PermissionPromptChip::~PermissionPromptChip() {
  FinalizeChip();
}

void PermissionPromptChip::UpdateAnchor() {
  UpdateBrowser();

  LocationBarView* lbv = GetLocationBarView();
  const bool is_location_bar_drawn =
      lbv && lbv->IsDrawn() && !lbv->GetWidget()->IsFullscreen();
  DCHECK(lbv->IsChipActive());

  if (!is_location_bar_drawn) {
    FinalizeChip();
    delegate()->RecreateView();
  }
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

  return permissions::PermissionUtil::ShouldPermissionBubbleStartOpen(
             delegate())
             ? permissions::PermissionPromptDisposition::
                   LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE
             : permissions::PermissionPromptDisposition::LOCATION_BAR_LEFT_CHIP;
}

views::Widget* PermissionPromptChip::GetPromptBubbleWidgetForTesting() {
  LocationBarView* lbv = GetLocationBarView();

  return lbv->IsChipActive() && lbv->chip()->IsBubbleShowing()
             ? lbv->chip()->GetPromptBubbleWidgetForTesting()  // IN-TEST
             : nullptr;
}

void PermissionPromptChip::FinalizeChip() {
  LocationBarView* lbv = GetLocationBarView();
  if (lbv && lbv->chip()) {
    lbv->FinalizeChip();
  }
}
