// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_TEST_BASE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"

class BrowserActions;
class TestingProfile;

class ActionAppMenuTestBase : public ChromeViewsTestBase {
 public:
  ActionAppMenuTestBase();
  ~ActionAppMenuTestBase() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  using MockActionCallback =
      testing::MockFunction<void(actions::ActionId,
                                 actions::ActionItem*,
                                 actions::ActionInvocationContext)>;

  std::unique_ptr<TestingProfile> profile_;
  testing::NiceMock<MockBrowserWindowInterface> mock_window_interface_;
  BrowserWindowFeatures features_;
  std::unique_ptr<BrowserActions> browser_actions_;
  raw_ptr<actions::ActionItem> root_action_ = nullptr;
  MockActionCallback mock_action_invoked_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_TEST_BASE_H_
