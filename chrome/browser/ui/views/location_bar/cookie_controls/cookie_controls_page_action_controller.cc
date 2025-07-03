// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_page_action_controller.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_observer.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
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
        bwi->GetFeatures().cookie_controls_bubble_coordinator();
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
// - Add IPH handling logic.
// - Implement the logic for executing the page action.
// - Add metrics reporting.
CookieControlsPageActionController::CookieControlsPageActionController(
    tabs::TabInterface& tab_interface,
    page_actions::PageActionController& page_action_controller)
    : page_action_controller_(page_action_controller),
      bubble_delegate_(std::make_unique<BubbleDelegateImpl>(tab_interface)) {
  CHECK(IsPageActionMigrated(PageActionIconType::kCookieControls));
}

CookieControlsPageActionController::~CookieControlsPageActionController() =
    default;

void CookieControlsPageActionController::OnCookieControlsIconStatusChanged(
    bool icon_visible,
    CookieControlsState controls_state,
    CookieBlocking3pcdStatus blocking_status,
    bool should_highlight) {
  if (!bubble_delegate_->IsReloading()) {
    return;
  }

  icon_status_ = CookieControlsIconStatus{
      .icon_visible = icon_visible,
      .controls_state = controls_state,
      .blocking_status = blocking_status,
      .should_highlight = should_highlight,
  };

  UpdatePageActionIcon(/*from_page_reload=*/false);
  if (icon_status_.controls_state == CookieControlsState::kBlocked3pc &&
      icon_status_.should_highlight && !bubble_delegate_->HasBubble()) {
    if (icon_status_.blocking_status != CookieBlocking3pcdStatus::kNotIn3pcd) {
      page_action_controller_->OverrideText(
          kActionShowCookieControls,
          l10n_util::GetStringUTF16(
              IDS_TRACKING_PROTECTION_PAGE_ACTION_SITE_NOT_WORKING_LABEL));
    }
    page_action_controller_->ShowSuggestionChip(
        kActionShowCookieControls, page_actions::SuggestionChipConfig{
                                       .should_animate = true,
                                       .should_announce_chip = true,
                                   });
  }
}

void CookieControlsPageActionController::
    OnFinishedPageReloadWithChangedSettings() {
  if (icon_status_.icon_visible) {
    if (base::FeatureList::IsEnabled(privacy_sandbox::kActUserBypassUx)) {
      UpdatePageActionIcon(/*from_page_reload=*/true);
      // Animate the label to provide a visual confirmation to the user that
      // their protection status on the site has changed.
      page_action_controller_->ShowSuggestionChip(kActionShowCookieControls,
                                                  {.should_animate = true});
    }
  }
}

void CookieControlsPageActionController::UpdatePageActionIcon(
    bool from_page_reload) {
  if (!icon_status_.icon_visible && !bubble_delegate_->HasBubble()) {
    page_action_controller_->HideSuggestionChip(kActionShowCookieControls);
    page_action_controller_->Hide(kActionShowCookieControls);
    return;
  }

  const std::u16string& label = l10n_util::GetStringUTF16(
      GetLabelForStatus(icon_status_.controls_state,
                        icon_status_.blocking_status, from_page_reload));
  page_action_controller_->OverrideImage(
      kActionShowCookieControls, ui::ImageModel::FromVectorIcon(GetVectorIcon(
                                     icon_status_.controls_state)));
  page_action_controller_->OverrideTooltip(kActionShowCookieControls, label);
  page_action_controller_->OverrideText(kActionShowCookieControls, label);
  page_action_controller_->Show(kActionShowCookieControls);
}
