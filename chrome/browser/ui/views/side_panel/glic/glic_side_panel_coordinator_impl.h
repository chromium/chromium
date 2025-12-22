// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_GLIC_GLIC_SIDE_PANEL_COORDINATOR_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_GLIC_GLIC_SIDE_PANEL_COORDINATOR_IMPL_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_scope.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/actions/actions.h"
#include "ui/views/view_tracker.h"

class SidePanelEntryScope;
class SidePanelRegistry;

namespace views {
class View;
}  // namespace views

namespace glic {

// GlicSidePanelCoordinatorImpl handles the creation and registration of the
// glic SidePanelEntry.
class GlicSidePanelCoordinatorImpl : public GlicSidePanelCoordinator,
                                     public SidePanelEntryObserver {
 public:
  GlicSidePanelCoordinatorImpl(tabs::TabInterface* tab,
                               SidePanelRegistry* side_panel_registry);
  ~GlicSidePanelCoordinatorImpl() override;

  // GlicSidePanelCoordinator:
  using GlicSidePanelCoordinator::Show;
  void Show(bool suppress_animations) override;
  void Close() override;
  bool IsShowing() const override;
  State state() override;
  base::CallbackListSubscription AddStateCallback(
      base::RepeatingCallback<void(State state)> callback) override;
  void SetContentsView(std::unique_ptr<views::View> contents_view) override;
  int GetPreferredWidth() override;
  bool IsGlicSidePanelActive() override;

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

  // Create and register the Glic side panel entry.
  void CreateAndRegisterEntry();

  // Returns the SidePanelCoordinator for the window associated with `tab_`.
  SidePanelCoordinator* GetWindowSidePanelCoordinator() const;

  // Gets the Glic WebView from the Glic service.
  std::unique_ptr<views::View> CreateView(SidePanelEntryScope& scope);

  // Sets a new state and notifies about a state change.
  void SetState(State new_state);

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

  base::WeakPtrFactory<GlicSidePanelCoordinatorImpl> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_GLIC_GLIC_SIDE_PANEL_COORDINATOR_IMPL_H_
