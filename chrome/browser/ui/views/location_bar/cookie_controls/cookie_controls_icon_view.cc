// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"

#include <memory>
#include <optional>

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view_impl.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/content_settings/core/common/cookie_controls_state.h"
#include "components/content_settings/core/common/features.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "content/public/browser/web_contents.h"
#include "cookie_controls_bubble_coordinator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace {

void RecordOpenedAction(bool icon_visible, CookieControlsState controls_state) {
  if (!icon_visible) {
    base::RecordAction(
        base::UserMetricsAction("CookieControls.Bubble.UnknownState.Opened"));
  } else if (controls_state == CookieControlsState::kBlocked3pc) {
    base::RecordAction(
        base::UserMetricsAction("CookieControls.Bubble.CookiesBlocked.Opened"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("CookieControls.Bubble.CookiesAllowed.Opened"));
  }
}
}  // namespace

CookieControlsIconView::CookieControlsIconView(
    Browser* browser,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "CookieControls"),
      browser_(browser),
      bubble_coordinator_(
          CHECK_DEREF(CookieControlsBubbleCoordinator::From(browser))) {
  CHECK(browser_);
  SetUpForInOutAnimation(/*duration=*/base::Seconds(12));
  SetBackgroundVisibility(BackgroundVisibility::kWithLabel);
  SetProperty(views::kElementIdentifierKey, kCookieControlsIconElementId);
}

CookieControlsIconView::~CookieControlsIconView() = default;

CookieControlsBubbleCoordinator&
CookieControlsIconView::GetCoordinatorForTesting() const {
  return bubble_coordinator_.get();
}

void CookieControlsIconView::SetCoordinatorForTesting(
    CookieControlsBubbleCoordinator& coordinator) {
  bubble_coordinator_ = coordinator;
}

void CookieControlsIconView::DisableUpdatesForTesting() {
  disable_updates_for_testing_ = true;
}

void CookieControlsIconView::UpdateImpl() {
  if (disable_updates_for_testing_) {
    return;
  }

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
              profile->IsIncognitoProfile());
      controller_observation_.Observe(controller_.get());
    }
    controller_->Update(web_contents);
  }
}

int CookieControlsIconView::GetLabelForState() const {
  return controls_state_ == CookieControlsState::kAllowed3pc
             ? IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_ALLOWED_LABEL
             : IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_BLOCKED_LABEL;
}

void CookieControlsIconView::SetLabelForState() {
  GetViewAccessibility().SetDescription(u"");
  SetLabel(l10n_util::GetStringUTF16(GetLabelForState()));
}

void CookieControlsIconView::UpdateTooltipText() {
  custom_tooltip_text_ = l10n_util::GetStringUTF16(GetLabelForState());
  SetTooltipText(custom_tooltip_text_);
}

std::u16string CookieControlsIconView::GetAlternativeAccessibleName() const {
  return custom_tooltip_text_.empty()
             ? PageActionIconView::GetAlternativeAccessibleName()
             : custom_tooltip_text_;
}

void CookieControlsIconView::OnCookieControlsIconStatusChanged(
    bool icon_visible,
    CookieControlsState controls_state) {
  // Always respect a change to the visibility of the icon, as this may happen
  // regardless of the controls state (e.g. the omnibox having or losing focus).
  icon_visible_ = icon_visible;
  if (!ShouldBeVisible()) {
    SetVisible(false);
    return;
  }
  SetVisible(true);

  // If the controls state has changed in some way, update the icon.
  if (controls_state != controls_state_) {
    controls_state_ = controls_state;
    UpdateIcon();
  }
}

void CookieControlsIconView::UpdateIcon() {
  UpdateIconImage();
  SetLabelForState();
  UpdateTooltipText();

  base::RecordAction(
      base::UserMetricsAction("TrackingProtection.UserBypass.Shown"));
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

  return icon_visible_;
}

bool CookieControlsIconView::GetAssociatedBubble() const {
  // There may be multiple icons but only a single bubble can be displayed
  // at a time. Check if the bubble belongs to this icon.
  return GetBubble() && GetBubble()->GetAnchorView() &&
         GetBubble()->GetAnchorView()->GetWidget() == GetWidget();
}

void CookieControlsIconView::ShowCookieControlsBubble() {
  bubble_coordinator_->ShowBubble(
      browser_->GetBrowserView().toolbar_button_provider(),
      delegate()->GetWebContentsForPageActionIconView(), controller_.get());
  CHECK(ShouldBeVisible());
  RecordOpenedAction(icon_visible_, controls_state_);
  base::RecordAction(
      base::UserMetricsAction("TrackingProtection.UserBypass.Shown.Opened"));
}

void CookieControlsIconView::OnExecuting(
    PageActionIconView::ExecuteSource source) {
  ShowCookieControlsBubble();
}

views::BubbleDialogDelegate* CookieControlsIconView::GetBubble() const {
  return bubble_coordinator_->GetBubble();
}

const gfx::VectorIcon& CookieControlsIconView::GetVectorIcon() const {
  return controls_state_ == CookieControlsState::kBlocked3pc
             ? features::IsRoundedIconsEnabled()
                   ? views::kVisibilityOffIcon
                   : views::kEyeCrossedRefreshOldIcon
         : features::IsRoundedIconsEnabled() ? views::kVisibilityIcon
                                             : views::kEyeRefreshOldIcon;
}

void CookieControlsIconView::UpdateTooltipForFocus() {}

BEGIN_METADATA(CookieControlsIconView)
ADD_READONLY_PROPERTY_METADATA(bool, AssociatedBubble)
END_METADATA
