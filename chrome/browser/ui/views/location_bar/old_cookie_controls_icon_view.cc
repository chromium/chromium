// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/old_cookie_controls_icon_view.h"

#include <memory>

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/location_bar/old_cookie_controls_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

OldCookieControlsIconView::OldCookieControlsIconView(
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "CookieControls") {
  SetVisible(false);
  SetAccessibilityProperties(
      /*role*/ absl::nullopt,
      l10n_util::GetStringUTF16(IDS_COOKIE_CONTROLS_TOOLTIP));
  SetProperty(views::kElementIdentifierKey, kCookieControlsIconElementId);
}

OldCookieControlsIconView::~OldCookieControlsIconView() = default;

void OldCookieControlsIconView::UpdateImpl() {
  auto* web_contents = delegate()->GetWebContentsForPageActionIconView();
  if (web_contents) {
    if (!controller_) {
      Profile* profile =
          Profile::FromBrowserContext(web_contents->GetBrowserContext());
      controller_ =
          std::make_unique<content_settings::CookieControlsController>(
              CookieSettingsFactory::GetForProfile(profile),
              profile->IsOffTheRecord() ? CookieSettingsFactory::GetForProfile(
                                              profile->GetOriginalProfile())
                                        : nullptr,
              HostContentSettingsMapFactory::GetForProfile(profile),
              /*tracking_protection_settings=*/nullptr);
      old_controller_observation_.Observe(controller_.get());
    }
    controller_->Update(web_contents);
  }
  SetVisible(ShouldBeVisible());
}

void OldCookieControlsIconView::OnStatusChanged(
    CookieControlsStatus status,
    CookieControlsEnforcement enforcement,
    int allowed_cookies,
    int blocked_cookies) {
  if (status_ != status) {
    status_ = status;
    SetVisible(ShouldBeVisible());
    UpdateIconImage();
  }

  OnCookiesCountChanged(allowed_cookies, blocked_cookies);
}

void OldCookieControlsIconView::OnCookiesCountChanged(int allowed_cookies,
                                                      int blocked_cookies) {
  // The blocked cookie count changes quite frequently, so avoid unnecessary
  // UI updates.
  if (has_blocked_cookies_ != blocked_cookies > 0) {
    has_blocked_cookies_ = blocked_cookies > 0;
    SetVisible(ShouldBeVisible());
  }
}

void OldCookieControlsIconView::OnStatefulBounceCountChanged(int bounce_count) {
  if (bounce_count > 0) {
    has_blocked_cookies_ = true;
    SetVisible(ShouldBeVisible());
  }
}

bool OldCookieControlsIconView::ShouldBeVisible() const {
  if (delegate()->ShouldHidePageActionIcons()) {
    return false;
  }

  if (GetAssociatedBubble()) {
    return true;
  }

  if (!delegate()->GetWebContentsForPageActionIconView()) {
    return false;
  }

  switch (status_) {
    case CookieControlsStatus::kDisabledForSite:
      return true;
    case CookieControlsStatus::kEnabled:
      // TODO(crbug.com/1446230): Update visibility logic, as part of task
      // b/285315102.
      return has_blocked_cookies_ || has_blocked_sites_;
    case CookieControlsStatus::kDisabled:
    case CookieControlsStatus::kUninitialized:
      return false;
  }
}

bool OldCookieControlsIconView::GetAssociatedBubble() const {
  // There may be multiple icons but only a single bubble can be displayed
  // at a time. Check if the bubble belongs to this icon.
  return GetBubble() && GetBubble()->GetAnchorView() &&
         GetBubble()->GetAnchorView()->GetWidget() == GetWidget();
}

void OldCookieControlsIconView::OnExecuting(
    PageActionIconView::ExecuteSource source) {
  OldCookieControlsBubbleView::ShowBubble(
      this, this, delegate()->GetWebContentsForPageActionIconView(),
      controller_.get(), status_);
}

views::BubbleDialogDelegate* OldCookieControlsIconView::GetBubble() const {
  return OldCookieControlsBubbleView::GetCookieBubble();
}

const gfx::VectorIcon& OldCookieControlsIconView::GetVectorIcon() const {
  if (OmniboxFieldTrial::IsChromeRefreshIconsEnabled()) {
    return status_ == CookieControlsStatus::kDisabledForSite
               ? views::kEyeRefreshIcon
               : views::kEyeCrossedRefreshIcon;
  }

  return status_ == CookieControlsStatus::kDisabledForSite
             ? views::kEyeIcon
             : views::kEyeCrossedIcon;
}

BEGIN_METADATA(OldCookieControlsIconView, PageActionIconView)
ADD_READONLY_PROPERTY_METADATA(bool, AssociatedBubble)
END_METADATA
