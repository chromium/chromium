// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_GLIC_GLIC_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_GLIC_GLIC_SIDE_PANEL_COORDINATOR_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_scope.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/actions/actions.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/view_tracker.h"

class SidePanelEntryScope;
class SidePanelRegistry;

namespace views {
class View;
}  // namespace views

namespace glic {

// GlicSidePanelCoordinator handles the creation and registration of the
// glic SidePanelEntry.
class GlicSidePanelCoordinator : public SidePanelEntryObserver {
 public:
  DECLARE_USER_DATA(GlicSidePanelCoordinator);

  GlicSidePanelCoordinator(tabs::TabInterface* tab,
                           SidePanelRegistry* side_panel_registry);
  ~GlicSidePanelCoordinator() override;

  static GlicSidePanelCoordinator* GetForTab(tabs::TabInterface* tab);

  // Create and register the Glic side panel entry.
  void CreateAndRegisterEntry();

  // The current state of the Glic side panel.
  enum class State {
    // The side panel is showing in the foreground.
    kShown,
    // The side panel is in the background, but it will show if its tab becomes
    // active.
    kBackgrounded,
    // The side panel is closed and will only be shown if explicitly requested.
    kClosed,
  };

  // Show the Glic side panel.
  void Show(bool suppress_animations = false);

  // Close the Glic side panel.
  void Close();

  // Returns true if the Glic side panel is currently the active entry.
  bool IsShowing() const;

  State state() { return state_; }

  // Registers `callback` to be called when panel visibility is updated.
  base::CallbackListSubscription AddStateCallback(
      base::RepeatingCallback<void(State state)> callback);

  // Sets the content view for the Glic side panel.
  void SetContentsView(std::unique_ptr<views::View> contents_view);

  // Returns preferred side panel width. Not guaranteed to be used if user
  // manually set a different width.
  int GetPreferredWidth();

  views::View* GetView();

  // Called when the Glic enabled status changes for `profile_`.
  void OnGlicEnabledChanged();

 protected:
  // SidePanelEntryObserver:
  void OnEntryWillHide(SidePanelEntry* entry,
                       SidePanelEntryHideReason reason) override;
  void OnEntryHideCancelled(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;
  void OnEntryShown(SidePanelEntry* entry) override;

 private:
  void CheckStateAfterHidden();

  // Returns the SidePanelCoordinator for the window associated with `tab_`.
  SidePanelCoordinator* GetWindowSidePanelCoordinator() const;

  // Gets the Glic WebView from the Glic service.
  std::unique_ptr<views::View> CreateView(SidePanelEntryScope& scope);

  void NotifyStateChanged();

  raw_ptr<tabs::TabInterface> tab_ = nullptr;
  raw_ptr<SidePanelRegistry> side_panel_registry_ = nullptr;
  base::WeakPtr<SidePanelEntry> entry_;
  base::CallbackListSubscription on_glic_enabled_changed_subscription_;
  base::RepeatingCallbackList<void(State state)> state_changed_callbacks_;

  State state_ = State::kClosed;

  std::optional<SidePanelEntryHideReason> pending_hide_reason_;

  // Tracks the glic container view.
  views::ViewTracker glic_container_tracker_;

  // Caches the contents view if it's set before the container is created.
  std::unique_ptr<views::View> contents_view_;

  base::WeakPtrFactory<GlicSidePanelCoordinator> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_GLIC_GLIC_SIDE_PANEL_COORDINATOR_H_
