// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_observer.h"
#include "components/tabs/public/tab_interface.h"

class SidePanelEntryScope;
class SidePanelRegistry;

namespace views {
class View;
}  // namespace views

class ReadAnythingSidePanelWebView;

// A per-tab class that facilitates the showing of the Read Anything side panel.
class ReadAnythingSidePanelController : public SidePanelEntryObserver {
 public:
  ReadAnythingSidePanelController(tabs::TabInterface* tab,
                                  SidePanelRegistry* side_panel_registry);
  ReadAnythingSidePanelController(const ReadAnythingSidePanelController&) =
      delete;
  ReadAnythingSidePanelController& operator=(
      const ReadAnythingSidePanelController&) = delete;
  ~ReadAnythingSidePanelController() override;

  // Removes the ReadAnythingControllerGlue from the web contents.
  void RemoveReadAnythingControllerGlue();

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;
  void OnEntryWillHide(SidePanelEntry* entry,
                       SidePanelEntryHideReason reason) override;

 private:
  void ReturnWebUIToController();

  // Creates the container view and all its child views for side panel entry.
  std::unique_ptr<views::View> CreateContainerView(SidePanelEntryScope& scope);

  // Returns the preferred default width for the side panel.
  int GetPreferredDefaultWidth();

  // Called when the tab will detach.
  void TabWillDetach(tabs::TabInterface* tab,
                     tabs::TabInterface::DetachReason reason);

  const raw_ptr<tabs::TabInterface> tab_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  // Holds the most recently created WebView for the side panel, if one exists.
  base::WeakPtr<ReadAnythingSidePanelWebView> web_view_;

  // Flag to indicate that the cached view should be cleared when hidden.
  bool should_clear_cached_view_on_hidden_ = false;

  // Must be the last member.
  base::WeakPtrFactory<ReadAnythingSidePanelController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_H_
