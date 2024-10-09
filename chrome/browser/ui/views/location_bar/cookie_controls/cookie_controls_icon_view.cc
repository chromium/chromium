// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"

#include <memory>
#include <optional>

#include "base/check_is_test.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "content/public/browser/web_contents.h"
#include "cookie_controls_bubble_coordinator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace {

void RecordOpenedAction(bool icon_visible, bool protections_on) {
  if (!icon_visible) {
    base::RecordAction(
        base::UserMetricsAction("CookieControls.Bubble.UnknownState.Opened"));
  } else if (protections_on) {
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
      browser_(browser) {
  CHECK(browser_);
  SetUpForInOutAnimation(/*duration=*/base::Seconds(12));
  SetBackgroundVisibility(BackgroundVisibility::kWithLabel);
  SetProperty(views::kElementIdentifierKey, kCookieControlsIconElementId);
  bubble_coordinator_ = std::make_unique<CookieControlsBubbleCoordinator>();
}

CookieControlsIconView::~CookieControlsIconView() = default;

CookieControlsBubbleCoordinator*
CookieControlsIconView::GetCoordinatorForTesting() const {
  return bubble_coordinator_.get();
}

void CookieControlsIconView::SetCoordinatorForTesting(
    std::unique_ptr<CookieControlsBubbleCoordinator> coordinator) {
  bubble_coordinator_ = std::move(coordinator);
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
              TrackingProtectionSettingsFactory::GetForProfile(profile));
      controller_observation_.Observe(controller_.get());
    }
    // Reset animation and tracker when URL changes.
    if (web_contents->GetVisibleURL() != last_visited_url_) {
      last_visited_url_ = web_contents->GetVisibleURL();
      did_animate_ = false;
      ResetSlideAnimation(false);
    }
    controller_->Update(web_contents);
  }
}

void CookieControlsIconView::MaybeShowIPH() {
  CHECK(browser_->window());
  user_education::FeaturePromoParams params(
      feature_engagement::kIPHCookieControlsFeature);
  params.show_promo_result_callback =
      base::BindOnce(&CookieControlsIconView::OnShowPromoResult,
                     weak_ptr_factory_.GetWeakPtr());
  params.close_callback = base::BindOnce(&CookieControlsIconView::OnIPHClosed,
                                         weak_ptr_factory_.GetWeakPtr());
  browser_->window()->MaybeShowFeaturePromo(std::move(params));
  // Note: originally we would animate here based on whether the promo showed,
  // but since promos are show asynchronously, the options are:
  //  - Always animate; if the IPH shows it shows
  //  - Always wait until we get a yes or no answer from the promo system before
  //    deciding whether to animate
  // Since most of the time the result should come back quickly, and if it
  // doesn't, it's because the user is doing something else or there is another
  // promo showing, for now, we choose the later option.
}

void CookieControlsIconView::OnShowPromoResult(
    user_education::FeaturePromoResult result) {
  if (result) {
    SetHighlighted(true);
    return;
  }
  // If we attempted to show the IPH but failed, instead try animating.
  MaybeAnimateIcon();
}

void CookieControlsIconView::OnIPHClosed() {
  SetHighlighted(false);
}

bool CookieControlsIconView::IsManagedIPHActive() const {
  CHECK(browser_->window());
  return browser_->window()->IsFeaturePromoActive(
      feature_engagement::kIPHCookieControlsFeature);
}

void CookieControlsIconView::SetLabelForStatus() {
  int icon_label = GetLabelForStatus();
  // Only use "Tracking Protection" and verbose accessibility description if the
  // label is hidden.
  if (ShouldShowTrackingProtectionText()) {
    // Set the accessible description to whatever the 3PC blocking state is.
    GetViewAccessibility().SetDescription(
        l10n_util::GetStringUTF16(icon_label));
    icon_label = IDS_TRACKING_PROTECTION_PAGE_ACTION_LABEL;
  } else {
    GetViewAccessibility().SetDescription(u"");
  }
  SetLabel(l10n_util::GetStringUTF16(icon_label));
}

int CookieControlsIconView::GetLabelForStatus() const {
  if (!protections_on_) {
    return IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_ALLOWED_LABEL;
  } else if (blocking_status_ == CookieBlocking3pcdStatus::kLimited) {
    return IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_LIMITED_LABEL;
  } else {
    return IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_BLOCKED_LABEL;
  }
}

void CookieControlsIconView::OnCookieControlsIconStatusChanged(
    bool icon_visible,
    bool protections_on,
    CookieBlocking3pcdStatus blocking_status,
    bool should_highlight) {
  if (icon_visible != icon_visible_ || protections_on != protections_on_ ||
      blocking_status != blocking_status_ || should_highlight_) {
    icon_visible_ = icon_visible;
    protections_changed_ = protections_on != protections_on_;
    protections_on_ = protections_on;
    blocking_status_ = blocking_status;
    should_highlight_ = should_highlight;
    UpdateIcon();
  }
}

void CookieControlsIconView::MaybeAnimateIcon() {
  if (GetAssociatedBubble() || IsManagedIPHActive() ||
      slide_animation_.is_animating()) {
    return;
  }

  int label = blocking_status_ == CookieBlocking3pcdStatus::kNotIn3pcd
                  ? GetLabelForStatus()
                  : IDS_TRACKING_PROTECTION_PAGE_ACTION_SITE_NOT_WORKING_LABEL;
  AnimateIn(label);
// VoiceOver on Mac already announces this text.
#if !BUILDFLAG(IS_MAC)
  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(label));
#endif
  if (controller_) {
    controller_->OnEntryPointAnimated();
  } else {
    CHECK_IS_TEST();
  }
  did_animate_ = true;
  base::RecordAction(
      base::UserMetricsAction("TrackingProtection.UserBypass.Animated"));
}

void CookieControlsIconView::UpdateIcon() {
  if (!ShouldBeVisible()) {
    ResetSlideAnimation(false);
    SetVisible(false);
    return;
  }
  UpdateIconImage();
  SetVisible(true);
  if (protections_changed_ || label()->GetText().empty()) {
    SetLabelForStatus();
  }
  SetTooltipText(
      l10n_util::GetStringUTF16(ShouldShowTrackingProtectionText()
                                    ? IDS_TRACKING_PROTECTION_PAGE_ACTION_LABEL
                                    : GetLabelForStatus()));
  if (protections_on_ && should_highlight_) {
    if (blocking_status_ == CookieBlocking3pcdStatus::kNotIn3pcd) {
      MaybeShowIPH();
    } else {
      MaybeAnimateIcon();
    }
  } else {
    base::RecordAction(
        base::UserMetricsAction("TrackingProtection.UserBypass.Shown"));
  }
}

void CookieControlsIconView::OnFinishedPageReloadWithChangedSettings() {
  // Do not attempt to change the visibility of the icon, only animate it, as
  // it should have already been visible for the user to have changed the
  // setting.
  if (ShouldBeVisible()) {
    GetViewAccessibility().SetDescription(u"");
    // Animate the icon to provide a visual confirmation to the user that their
    // protection status on the site has changed.
    AnimateIn(GetLabelForStatus());
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

  return icon_visible_;
}

bool CookieControlsIconView::GetAssociatedBubble() const {
  // There may be multiple icons but only a single bubble can be displayed
  // at a time. Check if the bubble belongs to this icon.
  return GetBubble() && GetBubble()->GetAnchorView() &&
         GetBubble()->GetAnchorView()->GetWidget() == GetWidget();
}

void CookieControlsIconView::ShowCookieControlsBubble() {
  CHECK(browser_->window());
  // Need to close IPH before opening bubble view, as on some platforms closing
  // the IPH bubble can cause activation to move between windows, and cookie
  // control bubble is close-on-deactivate.
  browser_->window()->NotifyFeaturePromoFeatureUsed(
      feature_engagement::kIPHCookieControlsFeature,
      FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  bubble_coordinator_->ShowBubble(
      delegate()->GetWebContentsForPageActionIconView(), controller_.get());
  CHECK(ShouldBeVisible());
  RecordOpenedAction(icon_visible_, protections_on_);
  if (did_animate_) {
    base::RecordAction(base::UserMetricsAction(
        "TrackingProtection.UserBypass.Animated.Opened"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("TrackingProtection.UserBypass.Shown.Opened"));
  }
}

void CookieControlsIconView::OnExecuting(
    PageActionIconView::ExecuteSource source) {
  ShowCookieControlsBubble();
}

views::BubbleDialogDelegate* CookieControlsIconView::GetBubble() const {
  return bubble_coordinator_->GetBubble();
}

const gfx::VectorIcon& CookieControlsIconView::GetVectorIcon() const {
  return protections_on_ ? views::kEyeCrossedRefreshIcon
                         : views::kEyeRefreshIcon;
}

bool CookieControlsIconView::ShouldShowTrackingProtectionText() {
  return base::FeatureList::IsEnabled(
             privacy_sandbox::kTrackingProtection3pcdUx) &&
         blocking_status_ != CookieBlocking3pcdStatus::kNotIn3pcd &&
         !label()->GetVisible();
}

void CookieControlsIconView::UpdateTooltipForFocus() {}

BEGIN_METADATA(CookieControlsIconView)
ADD_READONLY_PROPERTY_METADATA(bool, AssociatedBubble)
END_METADATA
