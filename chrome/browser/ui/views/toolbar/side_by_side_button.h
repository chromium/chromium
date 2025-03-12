// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_SIDE_BY_SIDE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_SIDE_BY_SIDE_BUTTON_H_

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

class SideBySideToolbarButton : public ToolbarButton, TabStripModelObserver {
  METADATA_HEADER(SideBySideToolbarButton, ToolbarButton)

 public:
  explicit SideBySideToolbarButton(Browser* browser);
  SideBySideToolbarButton(const SideBySideToolbarButton&) = delete;
  SideBySideToolbarButton& operator=(const SideBySideToolbarButton&) = delete;
  ~SideBySideToolbarButton() override;

  // TabStripModelObserver implementation:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnSplitViewAdded(std::vector<int> indices) override;

 private:
  void ButtonPressed(const ui::Event& event);

  void UpdateButtonVisibility();

  raw_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_SIDE_BY_SIDE_BUTTON_H_
