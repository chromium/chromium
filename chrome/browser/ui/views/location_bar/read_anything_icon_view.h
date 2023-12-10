// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_READ_ANYTHING_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_READ_ANYTHING_ICON_VIEW_H_

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"

class Browser;
class CommandUpdater;

// The location bar icon to open read anything.
class ReadAnythingIconView : public PageActionIconView,
                             public ReadAnythingCoordinator::Observer {
 public:
  METADATA_HEADER(ReadAnythingIconView);
  ReadAnythingIconView(
      CommandUpdater* command_updater,
      Browser* browser,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  ReadAnythingIconView(const ReadAnythingIconView&) = delete;
  ReadAnythingIconView& operator=(const ReadAnythingIconView&) = delete;
  ~ReadAnythingIconView() override;

 protected:
  // PageActionIconView:
  void UpdateImpl() override {}
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override {}
  void ExecuteCommand(ExecuteSource source) override;
  views::BubbleDialogDelegate* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  bool ShouldShowLabel() const override;

  // ReadAnythingCoordinator::Observer:
  void Activate(bool active) override;
  void OnActivePageDistillable(bool distillable) override;
  void OnCoordinatorDestroyed() override;

 private:
  const raw_ptr<Browser> browser_;
  raw_ptr<ReadAnythingCoordinator> coordinator_;
  base::ScopedObservation<ReadAnythingCoordinator,
                          ReadAnythingCoordinator::Observer>
      coordinator_observer_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_READ_ANYTHING_ICON_VIEW_H_
