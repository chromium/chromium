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
#include "content/public/browser/web_contents_observer.h"

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
class ReadAnythingSidePanelController : public SidePanelEntryObserver,
                                        public content::WebContentsObserver {
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

  // Decides whether the active page is distillable.
  bool IsActivePageDistillable() const;

  // Called when the associated tab enters the foreground.
  void TabForegrounded(tabs::TabInterface* tab);

  // Called when the tab will detach.
  void TabWillDetach(tabs::TabInterface* tab,
                     tabs::TabInterface::DetachReason reason);

  // content::WebContentsObserver:
  void DidStopLoading() override;
  void PrimaryPageChanged(content::Page& page) override;

  // Call this to update the visibility of the IPH. The foreground tab controls
  // visibility and background tabs do nothing.
  void UpdateIphVisibility();

  std::string default_language_code_;

  base::ObserverList<ReadAnythingSidePanelController::Observer> observers_;

  const raw_ptr<tabs::TabInterface> tab_;
  raw_ptr<SidePanelRegistry> side_panel_registry_;

  // The foreground tab is responsible for determining whether IPH is shown. IPH
  // should be shown if:
  // (1) the committed url is on an allow-list
  // (2) the page has finished loading
  // If (1) is true and the page is still loading, we use distillability from
  // the previously committed url to avoid sudden state changes.
  bool distillable_ = false;
  bool previous_page_distillable_ = false;
  bool loading_ = false;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  // Must be the last member.
  base::WeakPtrFactory<ReadAnythingSidePanelController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_CONTROLLER_H_
