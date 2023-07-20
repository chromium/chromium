// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"

#include <memory>

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/content_settings/core/common/cookie_controls_breakage_confidence_level.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CookieControlsIconView,
                                      kCookieControlsIcon);

CookieControlsIconView::CookieControlsIconView(
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "CookieControls") {
  SetUpForInOutAnimation(/*duration=*/base::Seconds(12));
  SetAccessibilityProperties(
      /*role*/ absl::nullopt,
      l10n_util::GetStringUTF16(IDS_COOKIE_CONTROLS_TOOLTIP));
  SetProperty(views::kElementIdentifierKey, kCookieControlsIcon);

  bubble_coordinator_ = std::make_unique<CookieControlsBubbleCoordinator>(this);
}

CookieControlsIconView::~CookieControlsIconView() = default;

CookieControlsBubbleCoordinator*
CookieControlsIconView::GetCoordinatorForTesting() const {
  return bubble_coordinator_.get();
}

void CookieControlsIconView::UpdateImpl() {
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
                                        : nullptr);
      controller_observation_.Observe(controller_.get());
    }
    controller_->Update(web_contents);
  }
  UpdateVisibilityAndAnimate();
}

void CookieControlsIconView::UpdateVisibilityAndAnimate(
    bool confidence_changed) {
  UpdateIconImage();
  bool should_show = ShouldBeVisible();
  if (should_show) {
    if (!GetVisible() || confidence_changed) {
      if (confidence_ == CookieControlsBreakageConfidenceLevel::kHigh) {
        AnimateIn(GetLabelForStatus());
      }
    }
  } else {
    UnpauseAnimation();
    ResetSlideAnimation(false);
  }
  SetVisible(should_show);
  SetTooltipText(l10n_util::GetStringUTF16(
      GetLabelForStatus().value_or(IDS_COOKIE_CONTROLS_TOOLTIP)));
}

absl::optional<int> CookieControlsIconView::GetLabelForStatus() const {
  switch (status_) {
    case CookieControlsStatus::kDisabledForSite:
      ABSL_FALLTHROUGH_INTENDED;
    case CookieControlsStatus::kDisabled:  // Cookies are not blocked
      return IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_ALLOWED_LABEL;
    case CookieControlsStatus::kEnabled:  // Cookies are blocked
      return IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_BLOCKED_LABEL;
    case CookieControlsStatus::kUninitialized:
      DLOG(ERROR) << "CookieControl status is not initialized";
      return absl::nullopt;
  }
}

void CookieControlsIconView::OnStatusChanged(
    CookieControlsStatus status,
    CookieControlsEnforcement enforcement,
    base::Time expiration) {
  if (status_ != status) {
    status_ = status;
    UpdateVisibilityAndAnimate();
  }
}

void CookieControlsIconView::OnSitesCountChanged(
    int allowed_third_party_sites_count,
    int blocked_third_party_sites_count) {
  // The icon doesn't update if sites count changes.
}

void CookieControlsIconView::OnBreakageConfidenceLevelChanged(
    CookieControlsBreakageConfidenceLevel level) {
  if (confidence_ != level) {
    confidence_ = level;
    UpdateVisibilityAndAnimate(/*confidence_changed=*/true);
  }
}

bool CookieControlsIconView::ShouldBeVisible() const {
  if (delegate()->ShouldHidePageActionIcons()) {
    return false;
  }

  if (GetAssociatedBubble()) {
    return true;
  }

  if (!delegate()->GetWebContentsForPageActionIconView()) {
    return false;
  }

  // Only show the icon for medium & high confidence.
  return (confidence_ == CookieControlsBreakageConfidenceLevel::kMedium ||
          confidence_ == CookieControlsBreakageConfidenceLevel::kHigh);
}

bool CookieControlsIconView::GetAssociatedBubble() const {
  // There may be multiple icons but only a single bubble can be displayed
  // at a time. Check if the bubble belongs to this icon.
  return GetBubble() && GetBubble()->GetAnchorView() &&
         GetBubble()->GetAnchorView()->GetWidget() == GetWidget();
}

void CookieControlsIconView::OnExecuting(
    PageActionIconView::ExecuteSource source) {
  bubble_coordinator_->ShowBubble(
      delegate()->GetWebContentsForPageActionIconView(), controller_.get());
}

views::BubbleDialogDelegate* CookieControlsIconView::GetBubble() const {
  return bubble_coordinator_->GetBubble();
}

const gfx::VectorIcon& CookieControlsIconView::GetVectorIcon() const {
  if (OmniboxFieldTrial::IsChromeRefreshIconsEnabled()) {
    return status_ == CookieControlsStatus::kDisabledForSite
               ? views::kEyeRefreshIcon
               : views::kEyeCrossedRefreshIcon;
  }

  return status_ == CookieControlsStatus::kDisabledForSite
             ? views::kEyeIcon
             : views::kEyeCrossedIcon;
}

BEGIN_METADATA(CookieControlsIconView, PageActionIconView)
ADD_READONLY_PROPERTY_METADATA(bool, AssociatedBubble)
END_METADATA
