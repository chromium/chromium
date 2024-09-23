// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_container.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/views/tabs/tab_organization_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/view_class_properties.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TriggerOutcome {
  kAccepted = 0,
  kDismissed = 1,
  kTimedOut = 2,
  kMaxValue = kTimedOut,
};

constexpr base::TimeDelta kExpansionInDuration = base::Milliseconds(500);
constexpr base::TimeDelta kExpansionOutDuration = base::Milliseconds(250);
constexpr base::TimeDelta kFlatEdgeInDuration = base::Milliseconds(400);
constexpr base::TimeDelta kFlatEdgeOutDuration = base::Milliseconds(250);
constexpr base::TimeDelta kOpacityInDuration = base::Milliseconds(300);
constexpr base::TimeDelta kOpacityOutDuration = base::Milliseconds(100);
constexpr base::TimeDelta kOpacityDelay = base::Milliseconds(100);
constexpr base::TimeDelta kShowDuration = base::Seconds(16);
constexpr char kTriggerOutcomeName[] = "Tab.Organization.Trigger.Outcome";

Edge GetFlatEdge(bool is_search_button, bool before_tab_strip) {
  const bool is_rtl = base::i18n::IsRTL();
  if ((!is_search_button && before_tab_strip) ||
      (is_search_button && !before_tab_strip)) {
    return is_rtl ? Edge::kRight : Edge::kLeft;
  }
  return is_rtl ? Edge::kLeft : Edge::kRight;
}

}  // namespace

TabSearchContainer::TabOrganizationAnimationSession::
    TabOrganizationAnimationSession(
        TabOrganizationButton* button,
        TabSearchContainer* container,
        AnimationSessionType session_type,
        base::OnceCallback<void()> on_animation_ended)
    : button_(button),
      container_(container),
      expansion_animation_(container),
      flat_edge_animation_(container),
      opacity_animation_(container),
      session_type_(session_type),
      on_animation_ended_(std::move(on_animation_ended)) {
  if (session_type_ == AnimationSessionType::HIDE) {
    expansion_animation_.Reset(1);
    flat_edge_animation_.Reset(1);
    opacity_animation_.Reset(1);
  }
}

TabSearchContainer::TabOrganizationAnimationSession::
    ~TabOrganizationAnimationSession() = default;

void TabSearchContainer::TabOrganizationAnimationSession::Start() {
  if (session_type_ ==
      TabOrganizationAnimationSession::AnimationSessionType::SHOW) {
    Show();
  } else {
    Hide();
  }
}

void TabSearchContainer::TabOrganizationAnimationSession::
    ResetAnimationForTesting(double value) {
  if (opacity_animation_delay_timer_.IsRunning()) {
    opacity_animation_delay_timer_.FireNow();
  }

  expansion_animation_.Reset(value);
  flat_edge_animation_.Reset(value);
  opacity_animation_.Reset(value);
}

void TabSearchContainer::TabOrganizationAnimationSession::Show() {
  expansion_animation_.SetTweenType(gfx::Tween::Type::ACCEL_20_DECEL_100);
  opacity_animation_.SetTweenType(gfx::Tween::Type::LINEAR);
  flat_edge_animation_.SetTweenType(gfx::Tween::Type::LINEAR);

  expansion_animation_.SetSlideDuration(
      GetAnimationDuration(kExpansionInDuration));
  flat_edge_animation_.SetSlideDuration(
      GetAnimationDuration(kFlatEdgeInDuration));
  opacity_animation_.SetSlideDuration(GetAnimationDuration(kOpacityInDuration));

  expansion_animation_.Show();
  flat_edge_animation_.Show();

  const base::TimeDelta delay = GetAnimationDuration(kOpacityDelay);
  opacity_animation_delay_timer_.Start(
      FROM_HERE, delay, this,
      &TabSearchContainer::TabOrganizationAnimationSession::
          ShowOpacityAnimation);
}

void TabSearchContainer::TabOrganizationAnimationSession::Hide() {
  // Animate and hide existing chip.
  if (session_type_ ==
      TabOrganizationAnimationSession::AnimationSessionType::SHOW) {
    if (opacity_animation_delay_timer_.IsRunning()) {
      opacity_animation_delay_timer_.FireNow();
    }
    session_type_ = TabOrganizationAnimationSession::AnimationSessionType::HIDE;
  }

  expansion_animation_.SetTweenType(gfx::Tween::Type::ACCEL_20_DECEL_100);
  opacity_animation_.SetTweenType(gfx::Tween::Type::LINEAR);
  flat_edge_animation_.SetTweenType(gfx::Tween::Type::ACCEL_20_DECEL_100);

  expansion_animation_.SetSlideDuration(
      GetAnimationDuration(kExpansionOutDuration));

  flat_edge_animation_.SetSlideDuration(
      GetAnimationDuration(kFlatEdgeOutDuration));

  opacity_animation_.SetSlideDuration(
      GetAnimationDuration(kOpacityOutDuration));

  expansion_animation_.Hide();
  flat_edge_animation_.Hide();
  opacity_animation_.Hide();
}

base::TimeDelta
TabSearchContainer::TabOrganizationAnimationSession::GetAnimationDuration(
    base::TimeDelta duration) {
  return gfx::Animation::ShouldRenderRichAnimation() ? duration
                                                     : base::TimeDelta();
}

void TabSearchContainer::TabOrganizationAnimationSession::
    ShowOpacityAnimation() {
  opacity_animation_.Show();
}

void TabSearchContainer::TabOrganizationAnimationSession::ApplyAnimationValue(
    const gfx::Animation* animation) {
  float value = animation->GetCurrentValue();
  if (animation == &expansion_animation_) {
    button_->SetWidthFactor(value);
  } else if (animation == &flat_edge_animation_) {
    container_->tab_search_button()->SetFlatEdgeFactor(1 - value);
    button_->SetFlatEdgeFactor(1 - value);
  } else if (animation == &opacity_animation_) {
    button_->SetOpacity(value);
  }
}

void TabSearchContainer::TabOrganizationAnimationSession::MarkAnimationDone(
    const gfx::Animation* animation) {
  if (animation == &expansion_animation_) {
    expansion_animation_done_ = true;
  } else if (animation == &flat_edge_animation_) {
    flat_edge_animation_done_ = true;
  } else {
    opacity_animation_done_ = true;
  }

  if (expansion_animation_done_ && flat_edge_animation_done_ &&
      opacity_animation_done_) {
    if (on_animation_ended_) {
      std::move(on_animation_ended_).Run();
    }
  }
}

TabSearchContainer::TabSearchContainer(
    TabStripController* tab_strip_controller,
    TabStripModel* tab_strip_model,
    bool before_tab_strip,
    View* locked_expansion_view,
    tabs::TabDeclutterController* tab_declutter_controller)
    : AnimationDelegateViews(this),
      locked_expansion_view_(locked_expansion_view),
      tab_declutter_controller_(tab_declutter_controller),
      tab_strip_model_(tab_strip_model) {
  mouse_watcher_ = std::make_unique<views::MouseWatcher>(
      std::make_unique<views::MouseWatcherViewHost>(locked_expansion_view,
                                                    gfx::Insets()),
      this);

  tab_organization_service_ = TabOrganizationServiceFactory::GetForProfile(
      tab_strip_controller->GetProfile());
  if (tab_organization_service_) {
    tab_organization_observation_.Observe(tab_organization_service_);
  }

  std::unique_ptr<TabSearchButton> tab_search_button =
      std::make_unique<TabSearchButton>(tab_strip_controller,
                                        GetFlatEdge(true, before_tab_strip));
  tab_search_button->SetProperty(views::kCrossAxisAlignmentKey,
                                 views::LayoutAlignment::kCenter);

  tab_search_button_ = AddChildView(std::move(tab_search_button));

  int tab_search_button_index = GetIndexOf(tab_search_button_).value();
  int index =
      before_tab_strip ? tab_search_button_index + 1 : tab_search_button_index;
  // TODO(crbug.com/40925230): Consider hiding the button when the request has
  // started, vs. when the button as clicked.
  auto_tab_group_button_ = AddChildViewAt(
      CreateAutoTabGroupButton(tab_strip_controller, before_tab_strip), index);

  SetupButtonProperties(auto_tab_group_button_, before_tab_strip);

  browser_ = tab_strip_controller->GetBrowser();

  // `tab_declutter_controller_` will be null for some profile types and if
  // feature is not enabled.
  if (tab_declutter_controller_) {
    tab_declutter_button_ = AddChildViewAt(
        CreateTabDeclutterButton(tab_strip_controller, before_tab_strip),
        index);

    SetupButtonProperties(tab_declutter_button_, before_tab_strip);

    tab_declutter_observation_.Observe(tab_declutter_controller_);
  }

  SetLayoutManager(std::make_unique<views::FlexLayout>());
}

TabSearchContainer::~TabSearchContainer() {
  if (scoped_tab_strip_modal_ui_) {
    scoped_tab_strip_modal_ui_.reset();
  }
}

void TabSearchContainer::SetupButtonProperties(TabOrganizationButton* button,
                                               bool before_tab_strip) {
  // Set the margins for the button
  const int space_between_buttons = 2;
  gfx::Insets margin;
  if (before_tab_strip) {
    margin.set_left(space_between_buttons);
  } else {
    margin.set_right(space_between_buttons);
  }
  button->SetProperty(views::kMarginsKey, margin);

  // Set opacity for the button
  button->SetOpacity(0);
}

std::unique_ptr<TabOrganizationButton>
TabSearchContainer::CreateAutoTabGroupButton(
    TabStripController* tab_strip_controller,
    bool before_tab_strip) {
  auto button = std::make_unique<TabOrganizationButton>(
      tab_strip_controller,
      base::BindRepeating(&TabSearchContainer::OnAutoTabGroupButtonClicked,
                          base::Unretained(this)),
      base::BindRepeating(&TabSearchContainer::OnAutoTabGroupButtonDismissed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_TAB_ORGANIZE),
      l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ORGANIZE),
      l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_ORGANIZE),
      kAutoTabGroupButtonElementId, GetFlatEdge(false, before_tab_strip));

  button->SetProperty(views::kCrossAxisAlignmentKey,
                      views::LayoutAlignment::kCenter);
  return button;
}

std::unique_ptr<TabOrganizationButton>
TabSearchContainer::CreateTabDeclutterButton(
    TabStripController* tab_strip_controller,
    bool before_tab_strip) {
  auto button = std::make_unique<TabOrganizationButton>(
      tab_strip_controller,
      base::BindRepeating(&TabSearchContainer::OnTabDeclutterButtonClicked,
                          base::Unretained(this)),
      base::BindRepeating(&TabSearchContainer::OnTabDeclutterButtonDismissed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_TAB_DECLUTTER),
      l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_DECLUTTER),
      l10n_util::GetStringUTF16(IDS_ACCNAME_TAB_DECLUTTER),
      kTabDeclutterButtonElementId, GetFlatEdge(false, before_tab_strip));

  button->SetProperty(views::kCrossAxisAlignmentKey,
                      views::LayoutAlignment::kCenter);
  return button;
}

void TabSearchContainer::ShowTabOrganization(TabOrganizationButton* button) {
  if (locked_expansion_view_->IsMouseHovered()) {
    SetLockedExpansionMode(LockedExpansionMode::kWillShow, button);
  }
  if (locked_expansion_mode_ == LockedExpansionMode::kNone) {
    ExecuteShowTabOrganization(button);
  }
}

void TabSearchContainer::HideTabOrganization(TabOrganizationButton* button) {
  if (locked_expansion_view_->IsMouseHovered()) {
    SetLockedExpansionMode(LockedExpansionMode::kWillHide, button);
  }
  if (locked_expansion_mode_ == LockedExpansionMode::kNone) {
    ExecuteHideTabOrganization(button);
  }
}

void TabSearchContainer::SetLockedExpansionModeForTesting(
    LockedExpansionMode mode,
    TabOrganizationButton* button) {
  SetLockedExpansionMode(mode, button);
}

void TabSearchContainer::OnAutoTabGroupButtonClicked() {
  base::UmaHistogramEnumeration(kTriggerOutcomeName, TriggerOutcome::kAccepted);
  tab_organization_service_->OnActionUIAccepted(browser_);

  UMA_HISTOGRAM_BOOLEAN("Tab.Organization.AllEntrypoints.Clicked", true);
  UMA_HISTOGRAM_BOOLEAN("Tab.Organization.Proactive.Clicked", true);

  // Force hide the button when pressed, bypassing locked expansion mode.
  ExecuteHideTabOrganization(auto_tab_group_button_);
}

void TabSearchContainer::OnAutoTabGroupButtonDismissed() {
  base::UmaHistogramEnumeration(kTriggerOutcomeName,
                                TriggerOutcome::kDismissed);
  tab_organization_service_->OnActionUIDismissed(browser_);

  UMA_HISTOGRAM_BOOLEAN("Tab.Organization.Proactive.Clicked", false);

  // Force hide the button when pressed, bypassing locked expansion mode.
  ExecuteHideTabOrganization(auto_tab_group_button_);
}

void TabSearchContainer::OnOrganizeButtonTimeout(
    TabOrganizationButton* button) {
  if (button == auto_tab_group_button_) {
    base::UmaHistogramEnumeration(kTriggerOutcomeName,
                                  TriggerOutcome::kTimedOut);
    UMA_HISTOGRAM_BOOLEAN("Tab.Organization.Proactive.Clicked", false);
  }

  // Hide the button if not pressed. Use locked expansion mode to avoid
  // disrupting the user.
  HideTabOrganization(button);
}

void TabSearchContainer::SetLockedExpansionMode(LockedExpansionMode mode,
                                                TabOrganizationButton* button) {
  if (mode == LockedExpansionMode::kNone) {
    if (locked_expansion_mode_ == LockedExpansionMode::kWillShow) {
      ExecuteShowTabOrganization(locked_expansion_button_);
    } else if (locked_expansion_mode_ == LockedExpansionMode::kWillHide) {
      ExecuteHideTabOrganization(locked_expansion_button_);
    }
    locked_expansion_button_ = nullptr;
  } else {
    locked_expansion_button_ = button;
    mouse_watcher_->Start(GetWidget()->GetNativeWindow());
  }
  locked_expansion_mode_ = mode;
}

void TabSearchContainer::ExecuteShowTabOrganization(
    TabOrganizationButton* button) {
  if (browser_ && (button == auto_tab_group_button_) &&
      !TabOrganizationUtils::GetInstance()->IsEnabled(browser_->profile())) {
    return;
  }

  // If the tab strip already has a modal UI showing, exit early.
  if (!tab_strip_model_->CanShowModalUI()) {
    return;
  }

  scoped_tab_strip_modal_ui_ = tab_strip_model_->ShowModalUI();

  animation_session_ = std::make_unique<TabOrganizationAnimationSession>(
      button, this, TabOrganizationAnimationSession::AnimationSessionType::SHOW,
      base::BindOnce(&TabSearchContainer::OnAnimationSessionEnded,
                     base::Unretained(this)));
  animation_session_->Start();

  hide_tab_organization_timer_.Start(
      FROM_HERE, kShowDuration,
      base::BindOnce(&TabSearchContainer::OnOrganizeButtonTimeout,
                     base::Unretained(this), button));
}

void TabSearchContainer::ExecuteHideTabOrganization(
    TabOrganizationButton* button) {
  // Hide the current animation if the shown button is the same button. Do not
  // create a new animation session.
  if (animation_session_ &&
      animation_session_->session_type() ==
          TabOrganizationAnimationSession::AnimationSessionType::SHOW &&
      animation_session_->button() == button) {
    hide_tab_organization_timer_.Stop();
    animation_session_->Hide();
    return;
  }

  if (!button->GetVisible()) {
    return;
  }

  // Stop the timer since the chip might be getting hidden on user actions like
  // dismissal or click and not timeout.
  hide_tab_organization_timer_.Stop();
  animation_session_ = std::make_unique<TabOrganizationAnimationSession>(
      button, this, TabOrganizationAnimationSession::AnimationSessionType::HIDE,
      base::BindOnce(&TabSearchContainer::OnAnimationSessionEnded,
                     base::Unretained(this)));
  animation_session_->Start();
}

void TabSearchContainer::MouseMovedOutOfHost() {
  SetLockedExpansionMode(LockedExpansionMode::kNone, nullptr);
}

void TabSearchContainer::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void TabSearchContainer::AnimationEnded(const gfx::Animation* animation) {
  animation_session_->ApplyAnimationValue(animation);
  animation_session_->MarkAnimationDone(animation);
}

void TabSearchContainer::OnAnimationSessionEnded() {
  // If the button went from shown -> hidden, unblock the tab strip from
  // showing other modal UIs.
  if (animation_session_->session_type() ==
      TabOrganizationAnimationSession::AnimationSessionType::HIDE) {
    scoped_tab_strip_modal_ui_.reset();
  }

  animation_session_.reset();
}

void TabSearchContainer::AnimationProgressed(const gfx::Animation* animation) {
  animation_session_->ApplyAnimationValue(animation);
}

void TabSearchContainer::OnToggleActionUIState(const Browser* browser,
                                               bool should_show) {
  CHECK(tab_organization_service_);

  if (locked_expansion_mode_ != LockedExpansionMode::kNone) {
    return;
  }

  if (should_show && browser_ == browser) {
    ShowTabOrganization(auto_tab_group_button_);
  } else {
    HideTabOrganization(auto_tab_group_button_);
  }
}

void TabSearchContainer::OnTabDeclutterButtonClicked() {
  const int tab_organization_tab_index = 1;
  tab_search_button_->tab_search_bubble_host()->ShowTabSearchBubble(
      false, tab_organization_tab_index,
      tab_search::mojom::TabOrganizationFeature::kDeclutter);

  // Force hide the button when pressed, bypassing locked expansion mode.
  ExecuteHideTabOrganization(tab_declutter_button_);
}

void TabSearchContainer::OnTabDeclutterButtonDismissed() {
  tab_declutter_controller_->OnActionUIDismissed(
      base::PassKey<TabSearchContainer>());

  // Force hide the button when pressed, bypassing locked expansion mode.
  ExecuteHideTabOrganization(tab_declutter_button_);
}

void TabSearchContainer::OnTriggerDeclutterUIVisibility(bool should_show) {
  CHECK(tab_declutter_controller_);
  if (locked_expansion_mode_ != LockedExpansionMode::kNone) {
    return;
  }

  if (should_show) {
    ShowTabOrganization(tab_declutter_button_);
  } else {
    HideTabOrganization(tab_declutter_button_);
  }
}

BEGIN_METADATA(TabSearchContainer)
END_METADATA
