// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_PERFORMANCE_CONTROLS_PERFORMANCE_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_PERFORMANCE_CONTROLS_PERFORMANCE_SIDE_PANEL_COORDINATOR_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/performance_controls/performance_state_observer.h"
#include "chrome/browser/ui/webui/side_panel/performance_controls/performance.mojom-shared.h"
#include "chrome/browser/ui/webui/side_panel/performance_controls/performance_side_panel_ui.h"

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

  void Show(std::vector<side_panel::mojom::PerformanceSidePanelNotification>
                notifications,
            SidePanelOpenTrigger open_trigger);

 private:
  friend class BrowserUserData<PerformanceSidePanelCoordinator>;

  std::unique_ptr<views::View> CreatePerformanceWebUIView();

  std::vector<side_panel::mojom::PerformanceSidePanelNotification>
      side_panel_notifications_;

  std::unique_ptr<PerformanceStateObserver> performance_state_observer_;

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_PERFORMANCE_CONTROLS_PERFORMANCE_SIDE_PANEL_COORDINATOR_H_
