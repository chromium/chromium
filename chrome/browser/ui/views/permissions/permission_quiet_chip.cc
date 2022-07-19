// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_quiet_chip.h"

#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/views/content_setting_bubble_contents.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_theme.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_style.h"
#include "components/permissions/permission_request.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/events/event.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {

const gfx::VectorIcon& GetBlockedPermissionIconId(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);

  return delegate->Requests()[0]->GetBlockedIconForChip();
}

std::u16string GetPermissionMessage(
    permissions::PermissionPrompt::Delegate* delegate) {
  DCHECK(delegate);
  DCHECK(delegate->Requests()[0]->GetQuietChipText().has_value());

  return delegate->Requests()[0]->GetQuietChipText().value();
}

}  // namespace

PermissionQuietChip::PermissionQuietChip(
    Browser* browser,
    permissions::PermissionPrompt::Delegate* delegate,
    bool should_expand)
    : browser_(browser), delegate_(delegate), should_expand_(should_expand) {
  DCHECK_EQ(1u, delegate->Requests().size());
  chip_shown_time_ = base::TimeTicks::Now();
}

PermissionQuietChip::~PermissionQuietChip() = default;

views::View* PermissionQuietChip::CreateBubble() {
  RecordChipButtonPressed();

  LocationBarView* lbv = GetLocationBarView();
  content::WebContents* web_contents = lbv->GetContentSettingWebContents();

  if (web_contents) {
    std::unique_ptr<ContentSettingQuietRequestBubbleModel>
        content_setting_bubble_model =
            std::make_unique<ContentSettingQuietRequestBubbleModel>(
                lbv->GetContentSettingBubbleModelDelegate(), web_contents);
    ContentSettingBubbleContents* quiet_request_bubble =
        new ContentSettingBubbleContents(
            std::move(content_setting_bubble_model), web_contents, lbv,
            views::BubbleBorder::TOP_LEFT);
    bubble_widget_ =
        views::BubbleDialogDelegateView::CreateBubble(quiet_request_bubble);

    quiet_request_bubble->set_close_on_deactivate(false);

    return quiet_request_bubble;
  }

  return nullptr;
}

void PermissionQuietChip::ShowBubble() {
  if (bubble_widget_) {
    bubble_widget_->Show();
  }
}

const gfx::VectorIcon& PermissionQuietChip::GetIconOn() {
  return GetBlockedPermissionIconId(delegate_);
}

const gfx::VectorIcon& PermissionQuietChip::GetIconOff() {
  return GetBlockedPermissionIconId(delegate_);
}

std::u16string PermissionQuietChip::GetMessage() {
  return GetPermissionMessage(delegate_);
}

bool PermissionQuietChip::ShouldStartOpen() {
  return should_bubble_start_open_;
}

bool PermissionQuietChip::ShouldExpand() {
  return should_expand_;
}

OmniboxChipTheme PermissionQuietChip::GetTheme() {
  return OmniboxChipTheme::kLowVisibility;
}

permissions::PermissionPrompt::Delegate*
PermissionQuietChip::GetPermissionPromptDelegate() {
  return delegate_;
}

void PermissionQuietChip::RecordChipButtonPressed() {
  base::UmaHistogramMediumTimes("Permissions.QuietChip.TimeToInteraction",
                                base::TimeTicks::Now() - chip_shown_time_);
}

LocationBarView* PermissionQuietChip::GetLocationBarView() {
  return BrowserView::GetBrowserViewForBrowser(browser_)->GetLocationBarView();
}
