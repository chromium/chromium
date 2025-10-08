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

  // Create and register the Glic side panel entry.
  void CreateAndRegisterEntry();

  using VisibilityCallback = base::RepeatingCallback<void(bool isVisible)>;

  // Registers `callback` to be called when panel visibility is updated.
  base::CallbackListSubscription AddVisibilityCallback(
      VisibilityCallback callback);

  // Set the content to display in the Glic side panel.
  void SetContentsView(std::unique_ptr<views::View> contents_view);

  // Returns preferred side panel width. Not guaranteed to be used if user
  // manually set a different width.
  int GetPreferredWidth();

  views::View* GetView();

 protected:
  // Called when the Glic enabled status changes for `profile_`.
  void OnGlicEnabledChanged();
  // `SidePanelEntryObserver`:
  void OnEntryWillHide(SidePanelEntry* entry,
                       SidePanelEntryHideReason reason) override;
  void OnEntryShown(SidePanelEntry* entry) override;

 private:
  // Gets the Glic WebView from the Glic service.
  std::unique_ptr<views::View> CreateView(SidePanelEntryScope& scope);

  raw_ptr<tabs::TabInterface> tab_ = nullptr;
  raw_ptr<SidePanelRegistry> side_panel_registry_ = nullptr;
  raw_ptr<actions::ActionItem> glic_action_ = nullptr;
  raw_ptr<SidePanelCoordinator> side_panel_coordinator_ = nullptr;
  base::CallbackListSubscription on_glic_enabled_changed_subscription_;
  base::RepeatingCallbackList<void(bool isShowing)>
      visibility_changed_callbacks_;
  std::unique_ptr<views::View> contents_view_;
  views::ViewTracker glic_container_tracker_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_GLIC_GLIC_SIDE_PANEL_COORDINATOR_H_
