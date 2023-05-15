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

  MOCK_METHOD(void, OnIssueUpdated, (const media_router::Issue* issue));
  MOCK_METHOD(void,
              OnRoutesUpdated,
              (const std::vector<media_router::MediaRoute>& routes));
  MOCK_METHOD(void, OnDialogShown, ());
  MOCK_METHOD(void, OnDialogHidden, ());
  MOCK_METHOD(void, OnContextMenuShown, ());
  MOCK_METHOD(void, OnContextMenuHidden, ());
  MOCK_METHOD(void, MaybeAddOrRemoveAction, ());
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_MOCK_MEDIA_ROUTER_ACTION_CONTROLLER_H_
