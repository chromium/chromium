// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZER_ORGANIZER_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZER_ORGANIZER_PANEL_CONTROLLER_H_

#include <cstdint>
#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_host.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserView;
class BrowserWindowInterface;
class OrganizerPanelView;

namespace content {
class WebContents;
}

namespace actions {
class ActionItem;
}  // namespace actions

namespace views {
class View;
}

// LINT.IfChange(OrganizerPanelCloseAction)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// See tools/metrics/histograms/metadata/tab/enums.xml.
enum class OrganizerPanelCloseAction {
  kNoAction = 0,
  kClosedTab = 1,
  kOpenRecentlyClosed = 2,
  kSwitchToTab = 3,
  kOpenCrossDeviceTab = 4,
  kCreateTabGroup = 5,
  kOpenTabGroup = 6,
  kOpenTabGroupMenu = 7,
  kOpenCrossDevicePromo = 8,
  kMaxValue = kOpenCrossDevicePromo,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:OrganizerPanelCloseAction)

// Manages the Organizer Panel, its animations, its placement in the browser,
// etc.
class OrganizerPanelController {
 public:
  DECLARE_USER_DATA(OrganizerPanelController);

  explicit OrganizerPanelController(BrowserWindowInterface& browser_window,
                                    actions::ActionItem* root_action_item);
  OrganizerPanelController(const OrganizerPanelController&) = delete;
  OrganizerPanelController& operator=(const OrganizerPanelController&) = delete;
  virtual ~OrganizerPanelController();

  static OrganizerPanelController* From(BrowserWindowInterface* browser_window);

  bool IsOrganizerPanelVisible() const;

  // Returns whether the organizer panel is currently visible and embedded
  // within the vertical tab strip.
  bool IsOrganizerPanelEmbeddedInVerticalTabStrip() const;

  void SetOrganizerVisible(bool visible, bool immediate = false);

  void SetCloseAction(OrganizerPanelCloseAction action);
  void IncrementNumTabsClosed(uint32_t count = 1);

  OrganizerPanelLocation GetCurrentOrganizerPanelLocation() const;

  using StateChangedCallback =
      base::RepeatingCallback<void(OrganizerPanelController*)>;
  base::CallbackListSubscription RegisterOnStateChanged(
      StateChangedCallback callback);

  // Returns the current WebContents for the organizer panel, if created.
  content::WebContents* web_contents() const { return web_contents_.get(); }
  site_engagement::SiteEngagementService::Helper* site_engagement_helper()
      const {
    return site_engagement_helper_.get();
  }

  // Sets the panel view and returns the old one, if any.
  std::unique_ptr<views::View> SetPanelView(
      base::PassKey<BrowserView>,
      std::unique_ptr<views::View> panel_view);
  std::unique_ptr<views::View> SetPanelViewForTesting(
      std::unique_ptr<views::View> panel_view);

  // Registers / unregisters the active panel view with the controller.
  void RegisterPanelView(OrganizerPanelView* panel_view);
  void UnregisterPanelView(OrganizerPanelView* panel_view);

 private:
  // Notifies subscribers when the is_visible_ or location state of the
  // Organizer Panel changes.
  void NotifyStateChanged();

  // Update the Organizer Button's Action Item (kActionToggleOrganizerPanel)
  // based on the Organizer Panel's is_visible_ state.
  void UpdateOrganizerActionItem();

  // Records close metrics for the current panel session and resets them.
  void RecordCloseMetrics();

  // Returns the motion required to get into the current target state.
  BrowserAnimationMotion GetTargetMotion() const;

  // Controls whether the Organizer Panel is visible.
  bool is_visible_ = false;
  OrganizerPanelCloseAction close_action_ =
      OrganizerPanelCloseAction::kNoAction;
  int num_tabs_closed_ = 0;

  content::WebContents* EnsureWebContentsCreated();
  void UpdatePanelWebContents();

  const raw_ref<BrowserWindowInterface> browser_window_;
  const raw_ptr<actions::ActionItem> root_action_item_;

  class PanelViewManager;
  const std::unique_ptr<PanelViewManager> panel_view_manager_;
  raw_ptr<OrganizerPanelView> panel_view_ = nullptr;
  base::CallbackListSubscription vertical_tab_strip_animation_subscription_;

  // Callback list for state changes to visibility or location.
  base::RepeatingCallbackList<void(OrganizerPanelController*)>
      on_state_changed_callback_list_;
  ui::ScopedUnownedUserData<OrganizerPanelController> scoped_unowned_user_data_;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<site_engagement::SiteEngagementService::Helper>
      site_engagement_helper_;

  base::WeakPtrFactory<OrganizerPanelController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZER_ORGANIZER_PANEL_CONTROLLER_H_
