// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_PERFORMANCE_CONTROLS_PERFORMANCE_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_PERFORMANCE_CONTROLS_PERFORMANCE_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "chrome/browser/ui/browser_user_data.h"

class Browser;
class SidePanelRegistry;

namespace views {
class View;
}  // namespace views

// PerformanceSidePanelCoordinator handles the creation and registration of the
// performance SidePanelEntry.
class PerformanceSidePanelCoordinator
    : public BrowserUserData<PerformanceSidePanelCoordinator> {
 public:
  explicit PerformanceSidePanelCoordinator(Browser* browser);
  ~PerformanceSidePanelCoordinator() override;

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

 private:
  friend class BrowserUserData<PerformanceSidePanelCoordinator>;

  std::unique_ptr<views::View> CreatePerformanceWebUIView();

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_PERFORMANCE_CONTROLS_PERFORMANCE_SIDE_PANEL_COORDINATOR_H_
