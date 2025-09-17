// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_page_action_controller.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_observer.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/cookie_controls_state.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/vector_icons.h"

namespace {

class BubbleDelegateImpl
    : public CookieControlsPageActionController::BubbleDelegate {
 public:
  explicit BubbleDelegateImpl(tabs::TabInterface& tab_interface)
      : tab_interface_(tab_interface) {}
  BubbleDelegateImpl(const BubbleDelegateImpl&) = delete;
  BubbleDelegateImpl& operator=(const BubbleDelegateImpl&) = delete;
  ~BubbleDelegateImpl() override = default;

  bool IsReloading() override {
    return GetBubbleCoordinator().IsReloadingState();
  }

  bool HasBubble() override { return GetBubbleCoordinator().GetBubble(); }

 private:
  CookieControlsBubbleCoordinator& GetBubbleCoordinator() {
    auto* const bwi = tab_interface_->GetBrowserWindowInterface();
    CHECK(bwi);
    CookieControlsBubbleCoordinator* const coordinator =
        CookieControlsBubbleCoordinator::From(bwi);
    CHECK(coordinator);
    return *coordinator;
  }

  const raw_ref<tabs::TabInterface> tab_interface_;
};

int GetLabelForStatus(CookieControlsState controls_state,
                      CookieBlocking3pcdStatus blocking_status,
                      bool from_page_reload) {
  switch (controls_state) {
    case CookieControlsState::kActiveTp:
      return from_page_reload
                 ? IDS_TRACKING_PROTECTIONS_PAGE_ACTION_PROTECTIONS_RESUMED_LABEL
                 : IDS_TRACKING_PROTECTIONS_PAGE_ACTION_PROTECTIONS_ENABLED_LABEL;
    case CookieControlsState::kPausedTp:
      return IDS_TRACKING_PROTECTIONS_PAGE_ACTION_PROTECTIONS_PAUSED_LABEL;
    case CookieControlsState::kAllowed3pc:
      return IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_ALLOWED_LABEL;
    default:
      return blocking_status == CookieBlocking3pcdStatus::kLimited
                 ? IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_LIMITED_LABEL
                 : IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_BLOCKED_LABEL;
  }
}

const gfx::VectorIcon& GetVectorIcon(CookieControlsState controls_state) {
  return controls_state == CookieControlsState::kBlocked3pc ||
                 controls_state == CookieControlsState::kActiveTp
             ? views::kEyeCrossedRefreshIcon
             : views::kEyeRefreshIcon;
}
}  // namespace

// TODO(crbug.com/376283777): This class needs further work to achieve full
// parity with the legacy page action, including:
// - Implement the logic for executing the page action.
// - Add metrics reporting.
// - Hook up to `CookieControlsController`.
CookieControlsPageActionController::CookieControlsPageActionController(
    tabs::TabInterface& tab_interface,
    page_actions::PageActionController& page_action_controller)
    : tab_(tab_interface),
      page_action_controller_(page_action_controller),
      bubble_delegate_(std::make_unique<BubbleDelegateImpl>(tab_interface)) {
  // TODO(crbug.com/376283777): A fresh icon status should be set once
  // `CookieControlsController` is hooked up to this.
  icon_status_.controls_state = CookieControlsState::kHidden;
  CHECK(IsPageActionMigrated(PageActionIconType::kCookieControls));
}

CookieControlsPageActionController::~CookieControlsPageActionController() =
    default;

void CookieControlsPageActionController::OnCookieControlsIconStatusChanged(
    bool icon_visible,
    CookieControlsState controls_state,
    CookieBlocking3pcdStatus blocking_status,
    bool should_highlight) {
  if (bubble_delegate_->IsReloading()) {
    return;
  }

  const bool controls_state_changed =
      controls_state != icon_status_.controls_state;
  const bool should_update_icon =
      controls_state_changed ||
      blocking_status != icon_status_.blocking_status ||
      should_highlight != icon_status_.should_highlight;
  icon_status_ = CookieControlsIconStatus{
      .icon_visible = icon_visible,
      .controls_state = controls_state,
      .blocking_status = blocking_status,
      .should_highlight = should_highlight,
  };

  UpdateIconVisibility();
  if (!should_update_icon) {
    return;
  }

  page_action_controller_->OverrideImage(
      kActionShowCookieControls, ui::ImageModel::FromVectorIcon(GetVectorIcon(
                                     icon_status_.controls_state)));

  const std::u16string label = GetLabelForState(/*from_page_reload=*/false);
  page_action_controller_->OverrideTooltip(kActionShowCookieControls, label);
  if (controls_state_changed) {
    page_action_controller_->OverrideText(kActionShowCookieControls, label);
  }

  if (!icon_status_.icon_visible || !icon_status_.should_highlight ||
      icon_status_.controls_state != CookieControlsState::kBlocked3pc) {
    return;
  }
  if (icon_status_.blocking_status == CookieBlocking3pcdStatus::kNotIn3pcd) {
    if (auto* user_education = BrowserUserEducationInterface::From(
            tab_->GetBrowserWindowInterface())) {
      MaybeShowIPH(*user_education);
    }
  } else if (!bubble_delegate_->HasBubble() && !IsManagedIPHActive()) {
    page_action_controller_->OverrideText(
        kActionShowCookieControls,
        l10n_util::GetStringUTF16(
            IDS_TRACKING_PROTECTION_PAGE_ACTION_SITE_NOT_WORKING_LABEL));
    page_action_controller_->ShowSuggestionChip(
        kActionShowCookieControls, page_actions::SuggestionChipConfig{
                                       .should_animate = true,
                                       .should_announce_chip = true,
                                   });
  }
}

void CookieControlsPageActionController::
    OnFinishedPageReloadWithChangedSettings() {
  if (ShouldShowIcon()) {
    const std::u16string label = GetLabelForState(/*from_page_reload=*/true);
    page_action_controller_->OverrideText(kActionShowCookieControls, label);
    page_action_controller_->OverrideTooltip(kActionShowCookieControls, label);
    // Animate the label to provide a visual confirmation to the user that
    // their protection status on the site has changed.
    // TODO(crbug.com/376283777): Ensure that Mac voiceover doesn't trigger
    // here.
    page_action_controller_->ShowSuggestionChip(
        kActionShowCookieControls,
        {.should_animate = true, .should_announce_chip = true});
  }
}

bool CookieControlsPageActionController::ShouldShowIcon() const {
  return icon_status_.icon_visible || bubble_delegate_->HasBubble();
}

void CookieControlsPageActionController::UpdateIconVisibility() {
  if (!ShouldShowIcon()) {
    page_action_controller_->HideSuggestionChip(kActionShowCookieControls);
    page_action_controller_->Hide(kActionShowCookieControls);
    return;
  }
  page_action_controller_->Show(kActionShowCookieControls);
}

std::u16string CookieControlsPageActionController::GetLabelForState(
    bool from_page_reload) const {
  return l10n_util::GetStringUTF16(
      GetLabelForStatus(icon_status_.controls_state,
                        icon_status_.blocking_status, from_page_reload));
}

bool CookieControlsPageActionController::IsManagedIPHActive() const {
  auto* user_education =
      BrowserUserEducationInterface::From(tab_->GetBrowserWindowInterface());
  if (!user_education) {
    return false;
  }
  return user_education->IsFeaturePromoActive(
             feature_engagement::kIPHCookieControlsFeature) ||
         user_education->IsFeaturePromoQueued(
             feature_engagement::kIPHCookieControlsFeature);
}

void CookieControlsPageActionController::OnShowPromoResult(
    user_education::FeaturePromoResult result) {
  if (result) {
    iph_activity_ =
        page_action_controller_->AddActivity(kActionShowCookieControls);
    return;
  }
  // If we attempted to show the IPH but failed, instead try animating.
  page_action_controller_->ShowSuggestionChip(
      kActionShowCookieControls, page_actions::SuggestionChipConfig{
                                     .should_animate = true,
                                     .should_announce_chip = true,
                                 });
}

void CookieControlsPageActionController::OnIPHClosed() {
  iph_activity_.reset();
}

void CookieControlsPageActionController::MaybeShowIPH(
    BrowserUserEducationInterface& user_education) {
  user_education::FeaturePromoParams params(
      feature_engagement::kIPHCookieControlsFeature);
  params.show_promo_result_callback =
      base::BindOnce(&CookieControlsPageActionController::OnShowPromoResult,
                     weak_ptr_factory_.GetWeakPtr());
  params.close_callback =
      base::BindOnce(&CookieControlsPageActionController::OnIPHClosed,
                     weak_ptr_factory_.GetWeakPtr());

  user_education.MaybeShowFeaturePromo(std::move(params));

  // Note: originally we would animate here based on whether the promo showed,
  // but since promos are show asynchronously, the options are:
  //  - Always animate; if the IPH shows it shows
  //  - Always wait until we get a yes or no answer from the promo system before
  //    deciding whether to animate
  // Since most of the time the result should come back quickly, and if it
  // doesn't, it's because the user is doing something else or there is another
  // promo showing, for now, we choose the later option.
}
