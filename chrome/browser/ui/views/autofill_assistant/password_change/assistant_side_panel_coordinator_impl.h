// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_SIDE_PANEL_COORDINATOR_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_SIDE_PANEL_COORDINATOR_IMPL_H_

#include "base/memory/raw_ptr.h"
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

  // AssistantSidePanelCoordinator
  bool Shown() override;
  views::View* SetView(std::unique_ptr<views::View> view) override;
  views::View* GetView() override;
  void RemoveView() override;
  void AddObserver(SidePanelEntryObserver* observer) override;

  // SidePanelEntryObserver
  void OnEntryHidden(SidePanelEntry* entry) override;

 private:
  std::unique_ptr<views::View> CreateSidePanelView();
  SidePanelCoordinator* SidePanelCoordinator();
  SidePanelRegistry* SidePanelRegistry();

  raw_ptr<views::View> side_panel_view_host_ = nullptr;
  // Used to store the view set by |SetView()| in case it is called before
  // |CreateSidePanelView|.
  std::unique_ptr<views::View> side_panel_view_child_ = nullptr;

  raw_ptr<content::WebContents> web_contents_;
};
#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ASSISTANT_SIDE_PANEL_COORDINATOR_H_
