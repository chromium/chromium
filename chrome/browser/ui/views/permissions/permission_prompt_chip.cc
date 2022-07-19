// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_chip.h"

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/permission_chip.h"
#include "components/permissions/features.h"
#include "content/public/browser/web_contents.h"

namespace {

bool ShouldBubbleStartOpen(PermissionPromptChip::Delegate* delegate) {
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionChipGestureSensitive)) {
    std::vector<permissions::PermissionRequest*> requests =
        delegate->Requests();
    const bool has_gesture =
        std::any_of(requests.begin(), requests.end(),
                    [](permissions::PermissionRequest* request) {
                      return request->GetGestureType() ==
                             permissions::PermissionRequestGestureType::GESTURE;
                    });
    if (has_gesture)
      return true;
  }
  if (base::FeatureList::IsEnabled(
          permissions::features::kPermissionChipRequestTypeSensitive)) {
    // Notifications and geolocation are targeted here because they are usually
    // not necessary for the website to function correctly, so they can safely
    // be given less prominence.
    std::vector<permissions::PermissionRequest*> requests =
        delegate->Requests();
    const bool is_geolocation_or_notifications = std::any_of(
        requests.begin(), requests.end(),
        [](permissions::PermissionRequest* request) {
          permissions::RequestType request_type = request->request_type();
          return request_type == permissions::RequestType::kNotifications ||
                 request_type == permissions::RequestType::kGeolocation;
        });
    if (!is_geolocation_or_notifications)
      return true;
  }
  return false;
}

}  // namespace

PermissionPromptChip::PermissionPromptChip(Browser* browser,
                                           content::WebContents* web_contents,
                                           Delegate* delegate)
    : PermissionPromptDesktop(browser, web_contents, delegate) {
  LocationBarView* lbv = GetLocationBarView();
  if (delegate->ShouldCurrentRequestUseQuietUI()) {
    lbv->DisplayQuietChip(
        delegate, !permissions::PermissionUiSelector::ShouldSuppressAnimation(
                      delegate->ReasonForUsingQuietUi()));
  } else {
    lbv->DisplayChip(delegate, ShouldBubbleStartOpen(delegate));
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

  return ShouldBubbleStartOpen(delegate())
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
