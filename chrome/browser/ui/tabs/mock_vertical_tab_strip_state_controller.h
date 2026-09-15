// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_MOCK_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_MOCK_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_

#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace tabs::test {

class MockVerticalTabStripStateController
    : public VerticalTabStripStateController {
 public:
  explicit MockVerticalTabStripStateController(BrowserWindowInterface& browser);
  ~MockVerticalTabStripStateController() override;

  MOCK_METHOD(void, SetDelegate, (Delegate*), (override));
  MOCK_METHOD(bool, ShouldDisplayVerticalTabs, (), (const, override));
  MOCK_METHOD(void, SetVerticalTabsEnabled, (bool), (override));
  MOCK_METHOD(std::unique_ptr<ScopedEnableStateLock>,
              GetEnableStateLock,
              (),
              (override));
  MOCK_METHOD(bool, IsCollapsed, (), (const, override));
  MOCK_METHOD(VerticalTabStripCollapseState,
              GetCollapseState,
              (),
              (const, override));
  MOCK_METHOD(void, RequestCollapse, (bool), (override));
  MOCK_METHOD(int, GetUncollapsedWidth, (), (const, override));
  MOCK_METHOD(void, SetUncollapsedWidth, (int), (override));
  MOCK_METHOD(bool, IsExpandOnHoverEnabled, (), (const, override));
  MOCK_METHOD(void, SetExpandOnHoverEnabled, (bool), (override));
  MOCK_METHOD(bool, IsResizing, (), (const, override));
  MOCK_METHOD(void, SetIsResizing, (bool), (override));
  MOCK_METHOD(const VerticalTabStripState&, GetState, (), (const, override));

  using VerticalTabStripStateController::NotifyCollapseChanged;
  using VerticalTabStripStateController::NotifyExpandOnHoverEnabledChanged;
  using VerticalTabStripStateController::NotifyModeChanged;
  using VerticalTabStripStateController::NotifyModeWillChange;
  using VerticalTabStripStateController::NotifyResizingChanged;
};

}  // namespace tabs::test

#endif  // CHROME_BROWSER_UI_TABS_MOCK_VERTICAL_TAB_STRIP_STATE_CONTROLLER_H_
