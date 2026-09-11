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
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_host.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"

DEFINE_USER_DATA(OrganizerPanelController);

// Handles the work of moving the panel view around to the correct host based on
// browser state. This will normally be in the organizer tray view or the
// vertical tab strip region view.
class OrganizerPanelController::PanelViewManager {
 public:
  PanelViewManager(OrganizerPanelController& controller,
                   BrowserWindowInterface& browser)
      : controller_(controller), browser_(browser) {}

  std::unique_ptr<views::View> SetPanelView(
      std::unique_ptr<views::View> panel_view) {
    std::unique_ptr<views::View> old_panel = RemovePanelView();
    if (panel_view) {
      AddPanelView(std::move(panel_view));
    }
    return old_panel;
  }

 private:
  // Removes the panel view from its current host.
  std::unique_ptr<views::View> RemovePanelView() {
    auto* const panel_view = panel_view_.view();
    if (!panel_view) {
      return nullptr;
    }
    auto* const parent = panel_view->parent();
    auto* const old_host = OrganizerPanelHost::FromView(parent);
    CHECK(old_host) << "View is not a pane host or in a panel host: "
                    << parent->GetClassName();
    return old_host->TakePanelView();
  }

  // Adds the panel view to the correct host for the current browser state.
  void AddPanelView(std::unique_ptr<views::View> panel_view) {
    CHECK(!panel_view_);
    auto* const host = OrganizerPanelHost::GetPreferredHost(*browser_);
    CHECK(host) << "Browser has no panel host.";
    panel_view_.SetView(panel_view.get());
    host->SetPanelView(std::move(panel_view));
  }

  const raw_ref<OrganizerPanelController> controller_;
  const raw_ref<BrowserWindowInterface> browser_;
  views::ViewTracker panel_view_;
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

void OrganizerPanelController::SetOrganizerVisible(bool visible) {
  if (is_visible_ == visible) {
    return;
  }

  is_visible_ = visible;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!is_visible_) {
    active_extension_id_.reset();
  }
#endif
  BrowserAnimationController::From(&*browser_window_)
      ->Start(OrganizerPanelAnimations::kOrganizerPanel,
              is_visible_ ? OrganizerPanelAnimations::kShow
                          : OrganizerPanelAnimations::kHide);

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

#if BUILDFLAG(ENABLE_EXTENSIONS)
void OrganizerPanelController::OpenForExtension(
    const extensions::ExtensionId& extension_id) {
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
