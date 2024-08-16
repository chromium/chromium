// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"

class SidePanelRegistry;

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace views {
class View;
}  // namespace views

class ReadAnythingUntrustedPageHandler;

// A per-tab class that facilitates the showing of the Read Anything side panel.
class ReadAnythingSidePanelController : public SidePanelEntryObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void Activate(bool active) {}
    virtual void OnSidePanelControllerDestroyed() = 0;
  };
  ReadAnythingSidePanelController(tabs::TabInterface* tab,
                                  SidePanelRegistry* side_panel_registry);
  ReadAnythingSidePanelController(const ReadAnythingSidePanelController&) =
      delete;
  ReadAnythingSidePanelController& operator=(
      const ReadAnythingSidePanelController&) = delete;
  ~ReadAnythingSidePanelController() override;

  // TODO(https://crbug.com/347770670): remove this.
  void ResetForTabDiscard();

  void AddPageHandlerAsObserver(
      base::WeakPtr<ReadAnythingUntrustedPageHandler> page_handler);
  void RemovePageHandlerAsObserver(
      base::WeakPtr<ReadAnythingUntrustedPageHandler> page_handler);

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

  void AddObserver(ReadAnythingSidePanelController::Observer* observer);
  void RemoveObserver(ReadAnythingSidePanelController::Observer* observer);

 private:
  // Creates the container view and all its child views for side panel entry.
  std::unique_ptr<views::View> CreateContainerView();

  // Called when the tab will detach.
  void TabWillDetach(tabs::TabInterface* tab,
                     tabs::TabInterface::DetachReason reason);

  std::string default_language_code_;

  base::ObserverList<ReadAnythingSidePanelController::Observer> observers_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  const raw_ptr<tabs::TabInterface> tab_;
  raw_ptr<SidePanelRegistry> side_panel_registry_;

  // Must be the last member.
  base::WeakPtrFactory<ReadAnythingSidePanelController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_H_
