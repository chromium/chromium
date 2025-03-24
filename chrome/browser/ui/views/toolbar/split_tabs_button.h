// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_SPLIT_TABS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_SPLIT_TABS_BUTTON_H_

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

namespace tabs {
enum class SplitTabLayout;
}  // namespace tabs

class SplitTabsToolbarButton : public ToolbarButton, TabStripModelObserver {
  METADATA_HEADER(SplitTabsToolbarButton, ToolbarButton)

 public:
  explicit SplitTabsToolbarButton(Browser* browser);
  SplitTabsToolbarButton(const SplitTabsToolbarButton&) = delete;
  SplitTabsToolbarButton& operator=(const SplitTabsToolbarButton&) = delete;
  ~SplitTabsToolbarButton() override;

  // TabStripModelObserver implementation:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnSplitTabCreated(std::vector<std::pair<tabs::TabInterface*, int>> tabs,
                         split_tabs::SplitTabId split_id,
                         TabStripModelObserver::SplitTabAddReason reason,
                         tabs::SplitTabLayout tab_layout) override;

 private:
  void ButtonPressed(const ui::Event& event);

  void UpdateButtonVisibility();

  raw_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_SPLIT_TABS_BUTTON_H_
