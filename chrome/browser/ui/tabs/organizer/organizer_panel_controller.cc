// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"

#include <utility>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/ui/actions/actions_util.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/browser/ui/views/animations/tab_strip_animations.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_host.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_util.h"
#endif

namespace {

// Respond to vertical tab strip collapse by hiding the panel.
void OnVerticalTabStripAnimation(
    OrganizerPanelController* panel_controller,
    const BrowserAnimationController* animation_controller,
    BrowserAnimationUpdate update) {
  if (update != BrowserAnimationUpdate::kStarted) {
    return;
  }
  const auto motion = animation_controller->GetCurrentMotion(
      TabStripAnimations::kVerticalTabStrip);
  if (motion == TabStripAnimations::kCollapseOnHover ||
      motion == TabStripAnimations::kCollapse) {
    panel_controller->SetOrganizerVisible(false);
  }
}

}  // namespace

DEFINE_USER_DATA(OrganizerPanelController);

// Handles the work of moving the panel view around to the correct host based on
// browser state. This will normally be in the organizer tray view or the
// vertical tab strip region view.
class OrganizerPanelController::PanelViewManager {
 public:
  PanelViewManager(OrganizerPanelController& controller,
                   BrowserWindowInterface& browser)
      : controller_(controller), browser_(browser) {
    if (auto* const state_controller =
            tabs::VerticalTabStripStateController::From(&*browser_)) {
      auto callback =
          base::BindRepeating(&PanelViewManager::OnVerticalTabStripModeChanged,
                              base::Unretained(this));
      tab_strip_subscriptions_.emplace_back(
          state_controller->RegisterOnModeChanged(
              base::IgnoreArgs<tabs::VerticalTabStripStateController*>(
                  callback)));
      tab_strip_subscriptions_.emplace_back(
          state_controller->RegisterOnCollapseChanged(
              base::IgnoreArgs<tabs::VerticalTabStripCollapseState>(callback)));
      tab_strip_subscriptions_.emplace_back(
          state_controller->RegisterOnExpandOnHoverEnabledChanged(
              base::IgnoreArgs<bool>(callback)));
    }
  }

  void OnVerticalTabStripModeChanged() {
    if (UpdatePanelViewHost()) {
      controller_->SetOrganizerVisible(false, /*immediate=*/true);
    }
  }

  std::unique_ptr<views::View> SetPanelView(
      std::unique_ptr<views::View> panel_view) {
    std::unique_ptr<views::View> old_panel = RemovePanelView();
    if (panel_view) {
      AddPanelView(std::move(panel_view));
    }
    return old_panel;
  }

  // Maybe moves the panel between hosts if the desired host has changed.
  // Returns true if changed, false otherwise.
  bool UpdatePanelViewHost() {
    if (!panel_view_) {
      return false;
    }

    // Determine where the panel should go.
    const auto new_location =
        OrganizerPanelHost::GetPreferredLocation(*browser_);
    CHECK_NE(OrganizerPanelLocation::kNone, new_location)
        << "No new host to move panel to.";
    if (current_location_ == new_location) {
      return false;
    }

    // Determine where the panel currently is.
    auto* const current_host = OrganizerPanelHost::FromView(panel_view_.view());
    CHECK(current_host) << "No current host to move panel from.";
    DCHECK_EQ(
        OrganizerPanelHost::GetHostForLocation(*browser_, current_location_),
        current_host)
        << "Sanity check failed: Current host is not expected host.";

    // Find the new host.
    auto* const desired_host =
        OrganizerPanelHost::GetHostForLocation(*browser_, new_location);
    CHECK(desired_host) << "Browser has no panel host.";
    CHECK_NE(desired_host, current_host)
        << "Location changed but host remained the same.";

    // Move the panel.
    current_location_ = new_location;
    desired_host->SetOrganizerPanelView(current_host->TakeOrganizerPanelView());
    return true;
  }

  OrganizerPanelLocation current_location() const { return current_location_; }

 private:
  // Removes the panel view from its current host.
  std::unique_ptr<views::View> RemovePanelView() {
    auto* const panel_view = panel_view_.view();
    panel_view_.SetView(nullptr);
    current_location_ = OrganizerPanelLocation::kNone;
    if (!panel_view) {
      return nullptr;
    }
    auto* const old_host = OrganizerPanelHost::FromView(panel_view);
    CHECK(old_host) << "Panel was present but not in a host.";
    return old_host->TakeOrganizerPanelView();
  }

  // Adds the panel view to the correct host for the current browser state.
  void AddPanelView(std::unique_ptr<views::View> panel_view) {
    CHECK(panel_view);
    CHECK(!panel_view_);
    CHECK_EQ(OrganizerPanelLocation::kNone, current_location_);
    current_location_ = OrganizerPanelHost::GetPreferredLocation(*browser_);
    CHECK_NE(OrganizerPanelLocation::kNone, current_location_)
        << "Browser has no panel host";
    panel_view_.SetView(panel_view.get());
    auto* const host =
        OrganizerPanelHost::GetHostForLocation(*browser_, current_location_);
    CHECK(host) << "Panel host is missing.";
    host->SetOrganizerPanelView(std::move(panel_view));
  }

  const raw_ref<OrganizerPanelController> controller_;
  const raw_ref<BrowserWindowInterface> browser_;
  std::vector<base::CallbackListSubscription> tab_strip_subscriptions_;
  views::ViewTracker panel_view_;
  OrganizerPanelLocation current_location_ = OrganizerPanelLocation::kNone;
};

OrganizerPanelController::OrganizerPanelController(
    BrowserWindowInterface& browser_window,
    actions::ActionItem* root_action_item)
    : browser_window_(browser_window),
      root_action_item_(root_action_item),
      panel_view_manager_(
          std::make_unique<PanelViewManager>(*this, browser_window)),
      scoped_unowned_user_data_(browser_window.GetUnownedUserDataHost(),
                                *this) {
  UpdateOrganizerActionItem();

  vertical_tab_strip_animation_subscription_ =
      BrowserAnimationController::From(&browser_window)
          ->Subscribe(TabStripAnimations::kVerticalTabStrip,
                      base::BindRepeating(&OnVerticalTabStripAnimation,
                                          base::Unretained(this)));
}

OrganizerPanelController::~OrganizerPanelController() = default;

// static
OrganizerPanelController* OrganizerPanelController::From(
    BrowserWindowInterface* browser_window) {
  return Get(browser_window->GetUnownedUserDataHost());
}

bool OrganizerPanelController::IsOrganizerPanelVisible() const {
  return is_visible_;
}

void OrganizerPanelController::SetOrganizerVisible(bool visible,
                                                   bool immediate) {
  if (is_visible_ == visible) {
    return;
  }

  is_visible_ = visible;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!is_visible_) {
    active_extension_id_.reset();
  }
#endif
  if (immediate) {
    BrowserAnimationController::From(&*browser_window_)
        ->Reset(OrganizerPanelAnimations::kOrganizerPanel,
                is_visible_ ? OrganizerPanelAnimations::kShow
                            : OrganizerPanelAnimations::kHide);
  } else {
    BrowserAnimationController::From(&*browser_window_)
        ->Start(OrganizerPanelAnimations::kOrganizerPanel,
                is_visible_ ? OrganizerPanelAnimations::kShow
                            : OrganizerPanelAnimations::kHide);
  }

  if (is_visible_) {
    last_opened_time_ = base::TimeTicks::Now();
  } else {
    base::TimeDelta open_duration = base::TimeTicks::Now() - last_opened_time_;
    base::UmaHistogramCustomCounts("Projects.ProjectsPanel.TimeOpen",
                                   open_duration.InSeconds(), 1,
                                   base::Minutes(5).InSeconds(), 50);
  }

  NotifyStateChanged();
}

OrganizerPanelLocation
OrganizerPanelController::GetCurrentOrganizerPanelLocation() const {
  return panel_view_manager_->current_location();
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void OrganizerPanelController::OpenForExtension(
    const extensions::ExtensionId& extension_id) {
  Profile* const profile = browser_window_->GetProfile();
  if (profile->IsOffTheRecord() &&
      !extensions::util::IsIncognitoEnabled(extension_id, profile)) {
    return;
  }

  if (is_visible_ && active_extension_id_ == extension_id) {
    return;
  }

  active_extension_id_ = extension_id;
  SetOrganizerVisible(true);
}

void OrganizerPanelController::ToggleForExtension(
    const extensions::ExtensionId& extension_id) {
  if (is_visible_ && active_extension_id_ == extension_id) {
    SetOrganizerVisible(false);
    return;
  }
  OpenForExtension(extension_id);
}

void OrganizerPanelController::CloseForExtension(
    const extensions::ExtensionId& extension_id) {
  if (!is_visible_ || active_extension_id_ != extension_id) {
    return;
  }
  SetOrganizerVisible(false);
}
#endif

base::CallbackListSubscription OrganizerPanelController::RegisterOnStateChanged(
    StateChangedCallback callback) {
  return on_state_changed_callback_list_.Add(std::move(callback));
}

std::unique_ptr<views::View> OrganizerPanelController::SetPanelView(
    base::PassKey<BrowserView>,
    std::unique_ptr<views::View> panel_view) {
  return panel_view_manager_->SetPanelView(std::move(panel_view));
}

std::unique_ptr<views::View> OrganizerPanelController::SetPanelViewForTesting(
    std::unique_ptr<views::View> panel_view) {
  return panel_view_manager_->SetPanelView(std::move(panel_view));
}

void OrganizerPanelController::NotifyStateChanged() {
  UpdateOrganizerActionItem();
  on_state_changed_callback_list_.Notify(this);
}

void OrganizerPanelController::UpdateOrganizerActionItem() {
  actions::ActionItem* organizer_action =
      actions::ActionManager::Get().FindAction(kActionToggleOrganizerPanel,
                                               root_action_item_);
  if (!organizer_action) {
    return;
  }

  const auto& text = IsOrganizerPanelVisible() ? IDS_HIDE_ORGANIZER_PANEL
                                               : IDS_VIEW_ORGANIZER_PANEL;
  const std::u16string title_and_tooltip =
      chrome::GetCleanTitleAndTooltipText(l10n_util::GetStringUTF16(text));
  organizer_action->SetText(title_and_tooltip);
  organizer_action->SetTooltipText(title_and_tooltip);
}
