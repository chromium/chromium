// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/browser/ui/cookie_controls_controller.h"
#include "components/content_settings/core/common/cookie_controls_breakage_confidence_level.h"
#include "components/content_settings/core/common/cookie_controls_status.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/vector_icons.h"
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
              HostContentSettingsMapFactory::GetForProfile(profile));
      controller_observation_.Observe(controller_.get());
    }
    controller_->Update(web_contents);
  }
  UpdateVisibilityAndAnimate();
}

void CookieControlsIconView::OnIPHClosed() {
  SetHighlighted(false);
}

void CookieControlsIconView::UpdateVisibilityAndAnimate(
    bool confidence_changed) {
  UpdateIconImage();
  bool should_show = ShouldBeVisible();
  if (should_show) {
    // TODO(crbug.com/1446230): Don't animate when the LHS toggle is used.
    if (!GetAssociatedBubble() && (!GetVisible() || confidence_changed)) {
      if (confidence_ == CookieControlsBreakageConfidenceLevel::kHigh) {
        bool promo_shown = false;
        SetVisible(true);
        CHECK(browser_->window());
        promo_shown = browser_->window()->MaybeShowFeaturePromo(
            feature_engagement::kIPHCookieControlsFeature,
            base::BindOnce(&CookieControlsIconView::OnIPHClosed,
                           weak_ptr_factory_.GetWeakPtr()));
        if (promo_shown) {
          SetHighlighted(true);
        } else {
          auto label = GetLabelForStatus();
          AnimateIn(label);
          if (label.has_value()) {
            GetViewAccessibility().AnnounceText(
                l10n_util::GetStringUTF16(label.value()));
          }
        }
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
    SetVisible(false);
  }
  SetVisible(should_show);
  SetLabel(l10n_util::GetStringUTF16(
      GetLabelForStatus().value_or(IDS_COOKIE_CONTROLS_TOOLTIP)));
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

void CookieControlsIconView::OnFinishedPageReloadWithChangedSettings() {
  // Do not attempt to change the visibility of the icon, only animate it, as
  // it should have already been visible for the user to have changed the
  // setting.
  AnimateIn(GetLabelForStatus());
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

BEGIN_METADATA(CookieControlsIconView, PageActionIconView)
ADD_READONLY_PROPERTY_METADATA(bool, AssociatedBubble)
END_METADATA
