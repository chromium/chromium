// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_SIDE_PANEL_COORDINATOR_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_SIDE_PANEL_COORDINATOR_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"

class SidePanelCoordinator;
class SidePanelRegistry;

class AssistantSidePanelCoordinatorImpl : public AssistantSidePanelCoordinator,
                                          public SidePanelEntryObserver {
 public:
  explicit AssistantSidePanelCoordinatorImpl(
      content::WebContents* web_contents);
  AssistantSidePanelCoordinatorImpl(const AssistantSidePanelCoordinatorImpl&) =
      delete;
  AssistantSidePanelCoordinatorImpl& operator=(
      const AssistantSidePanelCoordinatorImpl&) = delete;
  ~AssistantSidePanelCoordinatorImpl() override;

  // AssistantDisplayDelegate:
  // Sets the Assistant side panel view. This method takes ownership of the view
  // and returns a pointer to it, which can be used for later updates.
  views::View* SetView(std::unique_ptr<views::View> view) override;

  // Gets the current view rendered in the side panel. Returns `nullptr` if the
  // side panel is hidden or no view has been set.
  views::View* GetView() override;

  // Removes the current view rendered in the side panel and destroys it.
  void RemoveView() override;

  // AssistantSidePanelCoordinator:
  bool Shown() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // SidePanelEntryObserver:
  void OnEntryHidden(SidePanelEntry* entry) override;

 private:
  std::unique_ptr<views::View> CreateSidePanelView();
  SidePanelCoordinator* GetSidePanelCoordinator();
  SidePanelRegistry* GetSidePanelRegistry();

  raw_ptr<views::View> side_panel_view_host_ = nullptr;
  // Used to store the view set by |SetView()| in case it is called before
  // |CreateSidePanelView|.
  std::unique_ptr<views::View> side_panel_view_child_ = nullptr;

  raw_ptr<content::WebContents> web_contents_;

  // List of observers to the side panel.
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_SIDE_PANEL_COORDINATOR_IMPL_H_
