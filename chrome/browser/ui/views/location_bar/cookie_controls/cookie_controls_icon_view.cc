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
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/cookie_controls_breakage_confidence_level.h"
#include "components/content_settings/core/common/cookie_controls_status.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "content/public/browser/web_contents.h"
#include "cookie_controls_bubble_coordinator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace {

void RecordShownActionForConfidence(
    CookieControlsBreakageConfidenceLevel confidence) {
  if (confidence == CookieControlsBreakageConfidenceLevel::kHigh) {
    base::RecordAction(
        base::UserMetricsAction("CookieControls.HighConfidence.Shown"));
  } else if (confidence == CookieControlsBreakageConfidenceLevel::kMedium) {
    base::RecordAction(
        base::UserMetricsAction("CookieControls.MediumConfidence.Shown"));
  } else if (confidence == CookieControlsBreakageConfidenceLevel::kLow) {
    base::RecordAction(
        base::UserMetricsAction("CookieControls.LowConfidence.Shown"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("CookieControls.OtherConfidence.Shown"));
  }
}

void RecordOpenedActionForConfidence(
    CookieControlsBreakageConfidenceLevel confidence) {
  if (confidence == CookieControlsBreakageConfidenceLevel::kHigh) {
    base::RecordAction(
        base::UserMetricsAction("CookieControls.HighConfidence.Opened"));
  } else if (confidence == CookieControlsBreakageConfidenceLevel::kMedium) {
    base::RecordAction(
        base::UserMetricsAction("CookieControls.MediumConfidence.Opened"));
  } else if (confidence == CookieControlsBreakageConfidenceLevel::kLow) {
    base::RecordAction(
        base::UserMetricsAction("CookieControls.LowConfidence.Opened"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("CookieControls.OtherConfidence.Opened"));
  }
}

void RecordOpenedActionForStatus(CookieControlsStatus status) {
  switch (status) {
    case CookieControlsStatus::kEnabled:
      // Cookie blocking is enabled.
      base::RecordAction(base::UserMetricsAction(
          "CookieControls.Bubble.CookiesBlocked.Opened"));
      break;
    case CookieControlsStatus::kDisabled:
    case CookieControlsStatus::kDisabledForSite:
      // Cookie blocking is disabled.
      base::RecordAction(base::UserMetricsAction(
          "CookieControls.Bubble.CookiesAllowed.Opened"));
      break;
    case CookieControlsStatus::kUninitialized:
      base::RecordAction(
          base::UserMetricsAction("CookieControls.Bubble.UnknownState.Opened"));
      break;
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
  SetPaintLabelOverSolidBackground(true);
  SetAccessibilityProperties(
      /*role*/ absl::nullopt,
      l10n_util::GetStringUTF16(IDS_COOKIE_CONTROLS_TOOLTIP));
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
                                        : nullptr,
              HostContentSettingsMapFactory::GetForProfile(profile),
              TrackingProtectionSettingsFactory::GetForProfile(profile));
      controller_observation_.Observe(controller_.get());
    }
    controller_->Update(web_contents);
  }
  UpdateVisibilityAndAnimate();
}

bool CookieControlsIconView::MaybeShowIPH() {
  CHECK(browser_->window());
  // Need to make element visible or calls to show IPH will fail.
  SetVisible(true);

  const base::Feature* feature = nullptr;
  if (blocking_status_ != CookieBlocking3pcdStatus::kNotIn3pcd) {
    // Only show the reminder IPH if cookies are blocked on the current site.
    if (status_ != CookieControlsStatus::kEnabled) {
      return false;
    }
    // UB reminder IPH in 3PCD experiment.
    feature = &feature_engagement::kIPH3pcdUserBypassFeature;
  } else if (confidence_ == CookieControlsBreakageConfidenceLevel::kHigh) {
    // UB IPH on high-confidence site.
    feature = &feature_engagement::kIPHCookieControlsFeature;
  } else {
    // In all other cases return without trying to show IPH.
    return false;
  }

  CHECK(feature);
  user_education::FeaturePromoParams params(*feature);
  params.close_callback = base::BindOnce(&CookieControlsIconView::OnIPHClosed,
                                         weak_ptr_factory_.GetWeakPtr());
  if (!browser_->window()->MaybeShowFeaturePromo(std::move(params))) {
    return false;
  }
  SetHighlighted(true);
  return true;
}

void CookieControlsIconView::OnIPHClosed() {
  SetHighlighted(false);
}

void CookieControlsIconView::SetLabelAndTooltip() {
  auto icon_label = GetLabelForStatus().value_or(IDS_COOKIE_CONTROLS_TOOLTIP);
  // Only use "Tracking Protection" and verbose accessibility description if the
  // label is hidden.
  if (blocking_status_ != CookieBlocking3pcdStatus::kNotIn3pcd &&
      !label()->GetVisible()) {
    // Set the accessible description to whatever the 3PC blocking state is.
    SetAccessibleDescription(l10n_util::GetStringUTF16(icon_label));
    icon_label = IDS_TRACKING_PROTECTION_PAGE_ACTION_LABEL;
  } else {
    SetAccessibleDescription(u"");
  }
  SetTooltipText(l10n_util::GetStringUTF16(icon_label));
  SetLabel(l10n_util::GetStringUTF16(icon_label));
}

void CookieControlsIconView::UpdateVisibilityAndAnimate(
    bool confidence_changed) {
  bool should_be_visible = ShouldBeVisible();
  if (should_be_visible) {
    // TODO(crbug.com/1446230): Don't animate when the LHS toggle is used.
    if (!GetAssociatedBubble() && (!GetVisible() || confidence_changed)) {
      if (!MaybeShowIPH() &&
          confidence_ == CookieControlsBreakageConfidenceLevel::kHigh) {
        auto label = GetLabelForStatus();
        if (blocking_status_ != CookieBlocking3pcdStatus::kNotIn3pcd &&
            status_ == CookieControlsStatus::kEnabled) {
          label = IDS_TRACKING_PROTECTION_PAGE_ACTION_SITE_NOT_WORKING_LABEL;
        }
        AnimateIn(label);
// VoiceOver on Mac already announces this text.
#if !BUILDFLAG(IS_MAC)
        if (label.has_value()) {
          GetViewAccessibility().AnnounceText(
              l10n_util::GetStringUTF16(label.value()));
        }
#endif
        if (controller_) {
          controller_->OnEntryPointAnimated();
        } else {
          CHECK_IS_TEST();
        }
      }
      RecordShownActionForConfidence(confidence_);
    }
  } else {
    UnpauseAnimation();
    ResetSlideAnimation(false);
  }
  UpdateIconImage();
  SetVisible(should_be_visible);
  // The icon label should never change in response to confidence,
  // other than to animate the label (handled above).
  if (!confidence_changed) {
    SetLabelAndTooltip();
  }
}

absl::optional<int> CookieControlsIconView::GetLabelForStatus() const {
  switch (status_) {
    case CookieControlsStatus::kDisabledForSite:
      ABSL_FALLTHROUGH_INTENDED;
    case CookieControlsStatus::kDisabled:  // Cookies are not blocked
      return IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_ALLOWED_LABEL;
    case CookieControlsStatus::kEnabled:  // Cookies are blocked
      if (blocking_status_ == CookieBlocking3pcdStatus::kNotIn3pcd) {
        return IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_BLOCKED_LABEL;
      } else {
        return IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_LIMITED_LABEL;
      }
    case CookieControlsStatus::kUninitialized:
      DLOG(ERROR) << "CookieControl status is not initialized";
      return absl::nullopt;
  }
}

void CookieControlsIconView::OnStatusChanged(
    CookieControlsStatus status,
    CookieControlsEnforcement enforcement,
    CookieBlocking3pcdStatus blocking_status,
    base::Time expiration) {
  if (status_ != status || blocking_status_ != blocking_status) {
    status_ = status;
    blocking_status_ = blocking_status;
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

void CookieControlsIconView::OnFinishedPageReloadWithChangedSettings() {
  // Do not attempt to change the visibility of the icon, only animate it, as
  // it should have already been visible for the user to have changed the
  // setting.
  if (ShouldBeVisible()) {
    SetAccessibleDescription(u"");
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

  if (status_ == CookieControlsStatus::kDisabled) {
    // Don't show the icon if third-party cookies are enabled by default.
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

void CookieControlsIconView::ShowCookieControlsBubble() {
  bubble_coordinator_->ShowBubble(
      delegate()->GetWebContentsForPageActionIconView(), controller_.get());
  CHECK(browser_->window());
  browser_->window()->CloseFeaturePromo(
      feature_engagement::kIPH3pcdUserBypassFeature);
  browser_->window()->CloseFeaturePromo(
      feature_engagement::kIPHCookieControlsFeature);
  browser_->window()->NotifyFeatureEngagementEvent(
      feature_engagement::events::kCookieControlsBubbleShown);
  RecordOpenedActionForStatus(status_);
  RecordOpenedActionForConfidence(confidence_);
}

void CookieControlsIconView::OnExecuting(
    PageActionIconView::ExecuteSource source) {
  ShowCookieControlsBubble();
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

void CookieControlsIconView::UpdateTooltipForFocus() {}

BEGIN_METADATA(CookieControlsIconView, PageActionIconView)
ADD_READONLY_PROPERTY_METADATA(bool, AssociatedBubble)
END_METADATA
