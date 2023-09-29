// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_

#include <memory>
#include <string>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "content/public/browser/web_contents_observer.h"

class Browser;
class ReadAnythingController;
class SidePanelRegistry;
namespace views {
class View;
}  // namespace views

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingCoordinator
//
//  A class that coordinates the Read Anything feature. This class registers
//  itself as a SidePanelEntry. It creates and owns the Read Anything controller
//  and model. It also creates the Read Anything views when requested by the
//  Side Panel controller.
//  The coordinator acts as the external-facing API for the Read Anything
//  feature. Classes outside this feature should make calls to the coordinator.
//  This class has the same lifetime as the browser.
//
class ReadAnythingCoordinator : public BrowserUserData<ReadAnythingCoordinator>,
                                public SidePanelEntryObserver,
                                public TabStripModelObserver,
                                public content::WebContentsObserver,
                                public BrowserListObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void Activate(bool active) {}
    virtual void OnCoordinatorDestroyed() = 0;
    virtual void SetDefaultLanguageCode(const std::string& code) {}
  };

  explicit ReadAnythingCoordinator(Browser* browser);
  ~ReadAnythingCoordinator() override;

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);
  ReadAnythingController* GetController();
  ReadAnythingModel* GetModel();

  void AddObserver(ReadAnythingCoordinator::Observer* observer);
  void RemoveObserver(ReadAnythingCoordinator::Observer* observer);
  void AddModelObserver(ReadAnythingModel::Observer* observer);
  void RemoveModelObserver(ReadAnythingModel::Observer* observer);

 private:
  friend class BrowserUserData<ReadAnythingCoordinator>;
  friend class ReadAnythingCoordinatorTest;
  friend class ReadAnythingCoordinatorScreen2xDataCollectionModeTest;

  // Used during construction to initialize the model with saved user prefs.
  void InitModelWithUserPrefs();
  // Starts the delay for showing the IPH after the tab has changed.
  void StartPageChangeDelay();
  // Occurs when the timer set when changing tabs is finished.
  void OnTabChangeDelayComplete();

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

  // Callback passed to SidePanelCoordinator. This function creates the
  // container view and all its child views and returns it.
  std::unique_ptr<views::View> CreateContainerView();

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // content::WebContentsObserver:
  void DidStopLoading() override;
  void PrimaryPageChanged(content::Page& page) override;

  content::WebContents* GetActiveWebContents() const;

  // Attempts to show in product help for reading mode.
  void MaybeShowReadingModeSidePanelIPH();
  void CancelShowReadingModeSidePanelIPH();
  bool ShouldShowReadingModeSidePanelIPH() const;

  std::string default_language_code_;
  std::unique_ptr<ReadAnythingModel> model_;
  std::unique_ptr<ReadAnythingController> controller_;

  const base::flat_set<std::string> distillable_urls_;

  base::ObserverList<Observer> observers_;

  bool post_tab_change_delay_complete_ = true;
  base::RetainingOneShotTimer delay_timer_;

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

  BROWSER_USER_DATA_KEY_DECL();
};
#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_
