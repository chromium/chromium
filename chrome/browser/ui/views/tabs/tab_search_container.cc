// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_container.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service_factory.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/views/tabs/tab_organization_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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

constexpr char kTriggerOutcomeName[] = "TabOrganization.Trigger.Outcome";

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
                                       bool before_tab_strip,
                                       View* locked_expansion_view)
    : AnimationDelegateViews(this),
      locked_expansion_view_(locked_expansion_view) {
  mouse_watcher_ = std::make_unique<views::MouseWatcher>(
      std::make_unique<views::MouseWatcherViewHost>(locked_expansion_view,
                                                    gfx::Insets()),
      this);

  Profile* profile = tab_strip_controller->GetProfile();
  if (TabOrganizationUtils::GetInstance()->IsEnabled(profile)) {
    tab_organization_service_ = TabOrganizationServiceFactory::GetForProfile(
        tab_strip_controller->GetProfile());
  }

  std::unique_ptr<TabSearchButton> tab_search_button =
      std::make_unique<TabSearchButton>(
          tab_strip_controller, tab_organization_service_
                                    ? GetFlatEdge(true, before_tab_strip)
                                    : Edge::kNone);
  tab_search_button->SetProperty(views::kCrossAxisAlignmentKey,
                                 views::LayoutAlignment::kCenter);

  if (before_tab_strip) {
    tab_search_button_ = AddChildView(std::move(tab_search_button));
  }

  if (tab_organization_service_) {
    tab_organization_service_->AddObserver(this);
    // TODO(1469126): Consider hiding the button when the request has started,
    // vs. when the button as clicked.
    tab_organization_button_ =
        AddChildView(std::make_unique<TabOrganizationButton>(
            tab_strip_controller,
            base::BindRepeating(&TabSearchContainer::OnOrganizeButtonClicked,
                                base::Unretained(this)),
            base::BindRepeating(&TabSearchContainer::OnOrganizeButtonDismissed,
                                base::Unretained(this)),
            tab_organization_service_ ? GetFlatEdge(false, before_tab_strip)
                                      : Edge::kNone));
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
  }

  if (!before_tab_strip) {
    tab_search_button_ = AddChildView(std::move(tab_search_button));
  }

  browser_ = tab_strip_controller->GetBrowser();

  SetLayoutManager(std::make_unique<views::FlexLayout>());
}

TabSearchContainer::~TabSearchContainer() {
  if (tab_organization_service_) {
    tab_organization_service_->RemoveObserver(this);
  }
}

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
  expansion_animation_.Show();

  const base::TimeDelta delta = base::Seconds(16);
  hide_tab_organization_timer_.Start(
      FROM_HERE, delta, this, &TabSearchContainer::OnOrganizeButtonTimeout);
}

void TabSearchContainer::ExecuteHideTabOrganization() {
  expansion_animation_.Hide();
}

void TabSearchContainer::MouseMovedOutOfHost() {
  SetLockedExpansionMode(LockedExpansionMode::kNone);
}

void TabSearchContainer::AnimationCanceled(const gfx::Animation* animation) {
  ApplyAnimationValue(animation->GetCurrentValue());
}

void TabSearchContainer::AnimationEnded(const gfx::Animation* animation) {
  ApplyAnimationValue(animation->GetCurrentValue());
}

void TabSearchContainer::AnimationProgressed(const gfx::Animation* animation) {
  ApplyAnimationValue(animation->GetCurrentValue());
}

void TabSearchContainer::ApplyAnimationValue(float value) {
  tab_search_button_->SetFlatEdgeFactor(1 - value);
  tab_organization_button_->SetFlatEdgeFactor(1 - value);
  tab_organization_button_->SetWidthFactor(value);
}

void TabSearchContainer::OnToggleActionUIState(const Browser* browser,
                                               bool should_show) {
  CHECK(tab_organization_service_);
  if (should_show) {
    ShowTabOrganization();
  } else {
    HideTabOrganization();
  }
}

BEGIN_METADATA(TabSearchContainer, views::View)
END_METADATA
