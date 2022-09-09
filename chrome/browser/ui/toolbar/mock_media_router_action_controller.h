// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_MOCK_MEDIA_ROUTER_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOOLBAR_MOCK_MEDIA_ROUTER_ACTION_CONTROLLER_H_

#include "chrome/browser/ui/toolbar/media_router_action_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockMediaRouterActionController : public MediaRouterActionController {
 public:
  explicit MockMediaRouterActionController(Profile* profile);

  MockMediaRouterActionController(const MockMediaRouterActionController&) =
      delete;
  MockMediaRouterActionController& operator=(
      const MockMediaRouterActionController&) = delete;

  ~MockMediaRouterActionController() override;

  MOCK_METHOD1(OnIssueUpdated, void(const media_router::Issue* issue));
  MOCK_METHOD1(OnRoutesUpdated,
               void(const std::vector<media_router::MediaRoute>& routes));
  MOCK_METHOD0(OnDialogShown, void());
  MOCK_METHOD0(OnDialogHidden, void());
  MOCK_METHOD0(OnContextMenuShown, void());
  MOCK_METHOD0(OnContextMenuHidden, void());
  MOCK_METHOD0(MaybeAddOrRemoveAction, void());
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_MOCK_MEDIA_ROUTER_ACTION_CONTROLLER_H_
