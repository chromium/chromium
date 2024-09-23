// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_chip.h"

#include <algorithm>
#include <memory>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/request_type.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

PermissionPromptChip::PermissionPromptChip(Browser* browser,
                                           content::WebContents* web_contents,
                                           Delegate* delegate)
    : PermissionPromptDesktop(browser, web_contents, delegate),
      delegate_(delegate) {
  DCHECK(delegate_);
  LocationBarView* lbv = GetLocationBarView();

  // Before showing a chip make sure the LocationBar is in a valid state. That
  // fixes a bug when a chip overlays the padlock icon.
  lbv->InvalidateLayout();

  if (delegate->ShouldCurrentRequestUseQuietUI())
    PreemptivelyResolvePermissionRequest(web_contents, delegate);

  chip_controller_ = lbv->GetChipController();
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

  LocationBarView* lbv = GetLocationBarView();

  if (!lbv || !lbv->IsInitialized()) {
    return false;  // view should be recreated
  }

  const bool is_location_bar_drawn =
      lbv->IsDrawn() && !lbv->GetWidget()->IsFullscreen();
  if (chip_controller_->IsPermissionPromptChipVisible() &&
      !is_location_bar_drawn) {
    chip_controller_->ResetPermissionPromptChip();
    if (delegate_) {
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

  return permissions::PermissionPromptDisposition::
      LOCATION_BAR_LEFT_CHIP_AUTO_BUBBLE;
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
  LocationBarView* lbv = GetLocationBarView();

  return chip_controller_->IsPermissionPromptChipVisible() &&
                 lbv->GetChipController()->IsBubbleShowing()
             ? lbv->GetChipController()->GetBubbleWidget()
             : nullptr;
}

void PermissionPromptChip::PreemptivelyResolvePermissionRequest(
    content::WebContents* web_contents,
    Delegate* delegate) {
  if (base::FeatureList::IsEnabled(permissions::features::kFailFastQuietChip)) {
    DCHECK(delegate->ShouldCurrentRequestUseQuietUI());

    bool is_subscribed_to_permission_change_event = true;
    content::PermissionController* permission_controller =
        web_contents->GetBrowserContext()->GetPermissionController();

    // If at least one RFH is not subscribed to the PermissionChange event, we
    // should not preemptively resolve a prompt.
    for (permissions::PermissionRequest* request : delegate->Requests()) {
      content::RenderFrameHost* rfh =
          content::RenderFrameHost::FromID(request->get_requesting_frame_id());
      if (rfh == nullptr)
        return;

      ContentSettingsType type = request->GetContentSettingsType();

      blink::PermissionType permission_type =
          permissions::PermissionUtil::ContentSettingTypeToPermissionType(type);

      // Pre-ignore is allowed only for the quiet chip. The quiet chip is
      // enabled only for `NOTIFICATIONS` and `GEOLOCATION`.
      DCHECK(permission_type == blink::PermissionType::NOTIFICATIONS ||
             permission_type == blink::PermissionType::GEOLOCATION);

      is_subscribed_to_permission_change_event &=
          permission_controller->IsSubscribedToPermissionChangeEvent(
              permission_type, rfh);
    }

    if (is_subscribed_to_permission_change_event) {
      // This will resolve a promise so an origin is not waiting for the user's
      // decision.
      delegate->PreIgnoreQuietPrompt();
    }
  }
}
