// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_page_action_controller.h"

#include "base/callback_list.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/page_action/page_action_observer.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/cookie_controls_state.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/vector_icons.h"

namespace {

void RecordOpenedAction(CookieControlsState controls_state) {
  switch (controls_state) {
    case CookieControlsState::kHidden:
      base::RecordAction(
          base::UserMetricsAction("CookieControls.Bubble.UnknownState.Opened"));
      break;
    case CookieControlsState::kBlocked3pc:
      base::RecordAction(base::UserMetricsAction(
          "CookieControls.Bubble.CookiesBlocked.Opened"));
      break;
    case CookieControlsState::kAllowed3pc:
      base::RecordAction(base::UserMetricsAction(
          "CookieControls.Bubble.CookiesAllowed.Opened"));
      break;
  }
}

class BubbleDelegateImpl
    : public CookieControlsPageActionController::BubbleDelegate {
 public:
  explicit BubbleDelegateImpl(tabs::TabInterface& tab_interface)
      : tab_interface_(tab_interface) {}
  BubbleDelegateImpl(const BubbleDelegateImpl&) = delete;
  BubbleDelegateImpl& operator=(const BubbleDelegateImpl&) = delete;
  ~BubbleDelegateImpl() override = default;

  bool HasBubble() override {
    auto* coordinator = GetBubbleCoordinator();
    return coordinator && coordinator->GetBubble();
  }

  void ShowBubble(ToolbarButtonProvider* toolbar_button_provider,
                  content::WebContents* web_contents) override {
    auto* coordinator = GetBubbleCoordinator();
    if (!coordinator) {
      return;
    }
    auto* bwi = tab_interface_->GetBrowserWindowInterface();
    if (!bwi) {
      return;
    }
    return coordinator->ShowBubble(
        toolbar_button_provider, web_contents,
        bwi->GetFeatures().cookie_controls_controller());
  }

  base::CallbackListSubscription RegisterBubbleClosingCallback(
      base::RepeatingClosure callback) override {
    auto* coordinator = GetBubbleCoordinator();
    if (!coordinator) {
      return base::CallbackListSubscription();
    }
    return coordinator->RegisterBubbleClosingCallback(std::move(callback));
  }

  content_settings::CookieControlsController* GetController() override {
    auto* bwi = tab_interface_->GetBrowserWindowInterface();
    if (!bwi) {
      return nullptr;
    }
    return bwi->GetFeatures().cookie_controls_controller();
  }

 private:
  CookieControlsBubbleCoordinator* GetBubbleCoordinator() {
    auto* const bwi = tab_interface_->GetBrowserWindowInterface();
    if (!bwi) {
      return nullptr;
    }
    CookieControlsBubbleCoordinator* const coordinator =
        CookieControlsBubbleCoordinator::From(bwi);
    CHECK(coordinator);
    return coordinator;
  }

  const raw_ref<tabs::TabInterface> tab_interface_;
};

int GetLabelForStatus(CookieControlsState controls_state) {
  return controls_state == CookieControlsState::kAllowed3pc
             ? IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_ALLOWED_LABEL
             : IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_BLOCKED_LABEL;
}

const gfx::VectorIcon& GetVectorIcon(CookieControlsState controls_state) {
  return controls_state == CookieControlsState::kBlocked3pc
             ? features::IsRoundedIconsEnabled()
                   ? views::kVisibilityOffIcon
                   : views::kEyeCrossedRefreshOldIcon
         : features::IsRoundedIconsEnabled() ? views::kVisibilityIcon
                                             : views::kEyeRefreshOldIcon;
}
}  // namespace

DEFINE_USER_DATA(CookieControlsPageActionController);

CookieControlsPageActionController::CookieControlsPageActionController(
    tabs::TabInterface& tab_interface,
    Profile& profile,
    page_actions::PageActionController& page_action_controller)
    : PageActionObserver(kActionShowCookieControls),
      tab_(tab_interface),
      page_action_controller_(page_action_controller),
      bubble_delegate_(std::make_unique<BubbleDelegateImpl>(tab_interface)),
      scoped_unowned_user_data_(tab_interface.GetUnownedUserDataHost(), *this) {
  RegisterAsPageActionObserver(page_action_controller_.get());
}

CookieControlsPageActionController::~CookieControlsPageActionController() =
    default;

// static
CookieControlsPageActionController* CookieControlsPageActionController::From(
    tabs::TabInterface& tab) {
  return Get(tab.GetUnownedUserDataHost());
}

void CookieControlsPageActionController::Init() {
  // This will get updated naturally.
  controls_state_ = CookieControlsState::kHidden;

  did_activate_subscription_ = tab_->RegisterDidActivate(
      base::BindRepeating(&CookieControlsPageActionController::OnDidActivate,
                          base::Unretained(this)));

  will_deactivate_subscription_ = tab_->RegisterWillDeactivate(
      base::BindRepeating(&CookieControlsPageActionController::OnWillDeactivate,
                          base::Unretained(this)));

  will_discard_contents_subscription_ =
      tab_->RegisterWillDiscardContents(base::BindRepeating(
          &CookieControlsPageActionController::OnWillDiscardContents,
          base::Unretained(this)));

  tab_will_detach_subscription_ = tab_->RegisterWillDetach(base::BindRepeating(
      [](CookieControlsPageActionController* controller,
         tabs::TabInterface* tab,
         tabs::TabInterface::DetachReason detach_reason) {
        if (tab->IsActivated()) {
          controller->OnWillDeactivate(tab);
        }
      },
      base::Unretained(this)));

  bubble_will_close_subscription_ =
      bubble_delegate_->RegisterBubbleClosingCallback(base::BindRepeating(
          &CookieControlsPageActionController::OnBubbleClosed,
          base::Unretained(this)));

  if (tab_->IsActivated()) {
    OnDidActivate(&*tab_);
  }
}

void CookieControlsPageActionController::OnDidActivate(
    tabs::TabInterface* tab) {
  content_settings::CookieControlsController* controller =
      bubble_delegate_->GetController();
  if (!controller) {
    return;
  }
  if (!controller_observation_.IsObserving()) {
    controller_observation_.Observe(controller);
  }
  controller->Update(tab_->GetContents());
}

void CookieControlsPageActionController::OnWillDeactivate(
    tabs::TabInterface* tab) {
  content_settings::CookieControlsController* controller =
      bubble_delegate_->GetController();
  if (controller) {
    controller->OnBubbleCloseTriggered();
  }
  controller_observation_.Reset();
}

void CookieControlsPageActionController::OnWillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  if (tab->IsActivated()) {
    content_settings::CookieControlsController* controller =
        bubble_delegate_->GetController();
    if (controller) {
      controller->Update(new_contents);
    }
  }
}

void CookieControlsPageActionController::OnCookieControlsIconStatusChanged(
    CookieControlsState controls_state) {
  controls_state_ = controls_state;

  UpdateIconVisibility();

  page_action_controller_->OverrideImage(
      kActionShowCookieControls,
      ui::ImageModel::FromVectorIcon(GetVectorIcon(controls_state_)));

  const std::u16string label = GetLabelForState();
  page_action_controller_->OverrideTooltip(kActionShowCookieControls, label);
  page_action_controller_->OverrideText(kActionShowCookieControls, label);
}

bool CookieControlsPageActionController::ShouldShowIcon() const {
  return controls_state_ != CookieControlsState::kHidden ||
         bubble_delegate_->HasBubble();
}

void CookieControlsPageActionController::UpdateIconVisibility() {
  if (!ShouldShowIcon()) {
    page_action_controller_->Hide(kActionShowCookieControls);
    return;
  }
  page_action_controller_->Show(kActionShowCookieControls);
}

std::u16string CookieControlsPageActionController::GetLabelForState() const {
  return l10n_util::GetStringUTF16(GetLabelForStatus(controls_state_));
}

void CookieControlsPageActionController::OnBubbleClosed() {
  UpdateIconVisibility();
}

void CookieControlsPageActionController::ExecutePageAction(
    ToolbarButtonProvider* toolbar_button_provider) {
  CHECK(ShouldShowIcon());
  bubble_delegate_->ShowBubble(toolbar_button_provider, tab_->GetContents());
  RecordOpenedAction(controls_state_);
}
