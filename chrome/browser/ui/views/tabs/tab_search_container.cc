// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_container.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/views/tabs/tab_organization_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
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

TabSearchContainer::TabSearchContainer(TabStripController* tab_strip_controller,
                                       TabStripModel* tab_strip_model,
                                       bool before_tab_strip,
                                       View* locked_expansion_view)
    : AnimationDelegateViews(this),
      locked_expansion_view_(locked_expansion_view),
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
  tab_organization_button_ = AddChildViewAt(
      std::make_unique<TabOrganizationButton>(
          tab_strip_controller,
          base::BindRepeating(&TabSearchContainer::OnOrganizeButtonClicked,
                              base::Unretained(this)),
          base::BindRepeating(&TabSearchContainer::OnOrganizeButtonDismissed,
                              base::Unretained(this)),
          GetFlatEdge(false, before_tab_strip)),
      index);
  tab_organization_button_->SetProperty(views::kCrossAxisAlignmentKey,
                                        views::LayoutAlignment::kCenter);
  const int space_between_buttons = 2;
  gfx::Insets margin = gfx::Insets();
  if (before_tab_strip) {
    margin.set_left(space_between_buttons);
  } else {
    margin.set_right(space_between_buttons);
  }
  tab_organization_button_->SetProperty(views::kMarginsKey, margin);
  tab_organization_button_->SetOpacity(0);

  browser_ = tab_strip_controller->GetBrowser();

  expansion_animation_.SetTweenType(gfx::Tween::Type::ACCEL_20_DECEL_100);
  opacity_animation_.SetTweenType(gfx::Tween::Type::LINEAR);

  SetLayoutManager(std::make_unique<views::FlexLayout>());
}

TabSearchContainer::~TabSearchContainer() = default;

void TabSearchContainer::ShowTabOrganization() {
  if (locked_expansion_view_->IsMouseHovered()) {
    SetLockedExpansionMode(LockedExpansionMode::kWillShow);
  }
  if (locked_expansion_mode_ == LockedExpansionMode::kNone) {
    ExecuteShowTabOrganization();
  }
}

void TabSearchContainer::HideTabOrganization() {
  if (locked_expansion_view_->IsMouseHovered()) {
    SetLockedExpansionMode(LockedExpansionMode::kWillHide);
  }
  if (locked_expansion_mode_ == LockedExpansionMode::kNone) {
    ExecuteHideTabOrganization();
  }
}

void TabSearchContainer::SetLockedExpansionModeForTesting(
    LockedExpansionMode mode) {
  SetLockedExpansionMode(mode);
}

void TabSearchContainer::OnOrganizeButtonClicked() {
  base::UmaHistogramEnumeration(kTriggerOutcomeName, TriggerOutcome::kAccepted);
  tab_organization_service_->OnActionUIAccepted(browser_);

  UMA_HISTOGRAM_BOOLEAN("Tab.Organization.AllEntrypoints.Clicked", true);
  UMA_HISTOGRAM_BOOLEAN("Tab.Organization.Proactive.Clicked", true);

  // Force hide the button when pressed, bypassing locked expansion mode.
  ExecuteHideTabOrganization();
}

void TabSearchContainer::OnOrganizeButtonDismissed() {
  base::UmaHistogramEnumeration(kTriggerOutcomeName,
                                TriggerOutcome::kDismissed);
  tab_organization_service_->OnActionUIDismissed(browser_);

  UMA_HISTOGRAM_BOOLEAN("Tab.Organization.Proactive.Clicked", false);

  // Force hide the button when pressed, bypassing locked expansion mode.
  ExecuteHideTabOrganization();
}

void TabSearchContainer::OnOrganizeButtonTimeout() {
  base::UmaHistogramEnumeration(kTriggerOutcomeName, TriggerOutcome::kTimedOut);

  UMA_HISTOGRAM_BOOLEAN("Tab.Organization.Proactive.Clicked", false);

  // Hide the button if not pressed. Use locked expansion mode to avoid
  // disrupting the user.
  HideTabOrganization();
}

void TabSearchContainer::SetLockedExpansionMode(LockedExpansionMode mode) {
  if (mode == LockedExpansionMode::kNone) {
    if (locked_expansion_mode_ == LockedExpansionMode::kWillShow) {
      ExecuteShowTabOrganization();
    } else if (locked_expansion_mode_ == LockedExpansionMode::kWillHide) {
      ExecuteHideTabOrganization();
    }
  } else {
    mouse_watcher_->Start(GetWidget()->GetNativeWindow());
  }
  locked_expansion_mode_ = mode;
}

void TabSearchContainer::ExecuteShowTabOrganization() {
  // browser_ may be null in tests
  if (browser_ &&
      !TabOrganizationUtils::GetInstance()->IsEnabled(browser_->profile())) {
    return;
  }

  // If the tab strip already has a modal UI showing, exit early.
  if (!tab_strip_model_->CanShowModalUI()) {
    return;
  }

  scoped_tab_strip_modal_ui_ = tab_strip_model_->ShowModalUI();

  expansion_animation_.SetSlideDuration(
      GetAnimationDuration(kExpansionInDuration));

  flat_edge_animation_.SetSlideDuration(
      GetAnimationDuration(kFlatEdgeInDuration));
  flat_edge_animation_.SetTweenType(gfx::Tween::Type::LINEAR);

  opacity_animation_.SetSlideDuration(GetAnimationDuration(kOpacityInDuration));
  const base::TimeDelta delay = GetAnimationDuration(kOpacityDelay);
  opacity_animation_delay_timer_.Start(
      FROM_HERE, delay, this, &TabSearchContainer::ShowOpacityAnimation);

  expansion_animation_.Show();
  flat_edge_animation_.Show();

  hide_tab_organization_timer_.Start(
      FROM_HERE, kShowDuration, this,
      &TabSearchContainer::OnOrganizeButtonTimeout);
}

void TabSearchContainer::ShowOpacityAnimation() {
  opacity_animation_.Show();
}

void TabSearchContainer::ExecuteHideTabOrganization() {
  expansion_animation_.SetSlideDuration(
      GetAnimationDuration(kExpansionOutDuration));
  expansion_animation_.Hide();

  flat_edge_animation_.SetSlideDuration(
      GetAnimationDuration(kFlatEdgeOutDuration));
  flat_edge_animation_.SetTweenType(gfx::Tween::Type::ACCEL_20_DECEL_100);
  flat_edge_animation_.Hide();

  opacity_animation_.SetSlideDuration(
      GetAnimationDuration(kOpacityOutDuration));
  opacity_animation_.Hide();
}

void TabSearchContainer::MouseMovedOutOfHost() {
  SetLockedExpansionMode(LockedExpansionMode::kNone);
}

void TabSearchContainer::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void TabSearchContainer::AnimationEnded(const gfx::Animation* animation) {
  ApplyAnimationValue(animation);
  // If the button went from shown -> hidden, unblock the tab strip from
  // showing other modal UIs. Compare to 0.5 to distinguish between show/hide
  // while avoiding potentially inexact float comparison to 0.0.
  if (animation == &expansion_animation_ &&
      animation->GetCurrentValue() < 0.5 && scoped_tab_strip_modal_ui_) {
    scoped_tab_strip_modal_ui_.reset();
  }
}

void TabSearchContainer::AnimationProgressed(const gfx::Animation* animation) {
  ApplyAnimationValue(animation);
}

void TabSearchContainer::ApplyAnimationValue(const gfx::Animation* animation) {
  float value = animation->GetCurrentValue();
  if (animation == &expansion_animation_) {
    tab_organization_button_->SetWidthFactor(value);
  } else if (animation == &flat_edge_animation_) {
    tab_search_button_->SetFlatEdgeFactor(1 - value);
    tab_organization_button_->SetFlatEdgeFactor(1 - value);
  } else if (animation == &opacity_animation_) {
    tab_organization_button_->SetOpacity(value);
  }
}

base::TimeDelta TabSearchContainer::GetAnimationDuration(
    base::TimeDelta duration) {
  return gfx::Animation::ShouldRenderRichAnimation() ? duration
                                                     : base::TimeDelta();
}

void TabSearchContainer::OnToggleActionUIState(const Browser* browser,
                                               bool should_show) {
  CHECK(tab_organization_service_);
  if (should_show && browser_ == browser) {
    ShowTabOrganization();
  } else {
    HideTabOrganization();
  }
}

BEGIN_METADATA(TabSearchContainer)
END_METADATA
