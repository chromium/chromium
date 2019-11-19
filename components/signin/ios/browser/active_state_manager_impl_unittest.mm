// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/active_state_manager_impl.h"

#include "components/signin/ios/browser/active_state_manager.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/test/web_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A test fixture to test ActiveStateManagerImpl.
typedef web::WebTest ActiveStateManagerImplTest;

// An ActiveStateManager::Observer used for testing purposes.
class ActiveStateManagerObserver : public ActiveStateManager::Observer {
 public:
  ActiveStateManagerObserver() {}
  virtual ~ActiveStateManagerObserver() {}

  // ActiveStateManager::Observer implementation.
  MOCK_METHOD0(OnActive, void());
  MOCK_METHOD0(OnInactive, void());
  MOCK_METHOD0(WillBeDestroyed, void());
};

// Tests that an ActiveStateManagerImpl is successfully created with a
// BrowserState and that it can be made active/inactive.
TEST_F(ActiveStateManagerImplTest, ActiveState) {
  ActiveStateManager* active_state_manager =
      ActiveStateManager::FromBrowserState(GetBrowserState());
  ASSERT_TRUE(active_state_manager);

  active_state_manager->SetActive(true);
  EXPECT_TRUE(active_state_manager->IsActive());

  // Make sure it is ok to SetActive(true) on an already active
  // ActiveStateManager.
  active_state_manager->SetActive(true);
  EXPECT_TRUE(active_state_manager->IsActive());

  active_state_manager->SetActive(false);
  EXPECT_FALSE(active_state_manager->IsActive());
}

// Tests that ActiveStateManager::Observer are notified correctly.
TEST_F(ActiveStateManagerImplTest, ObserverMethod) {
  // |GetBrowserState()| already has its ActiveStateManager be active.
  ActiveStateManager::FromBrowserState(GetBrowserState())->SetActive(false);

  ActiveStateManagerObserver observer;
  web::TestBrowserState browser_state;
  ActiveStateManager* active_state_manager =
      ActiveStateManager::FromBrowserState(&browser_state);

  active_state_manager->AddObserver(&observer);

  EXPECT_CALL(observer, OnActive()).Times(1);
  EXPECT_CALL(observer, OnInactive()).Times(1);
  EXPECT_CALL(observer, WillBeDestroyed()).Times(1);

  active_state_manager->SetActive(true);
  active_state_manager->SetActive(false);
  // There is no need to explicitly remove the observer since it is removed when
  // |active_state_manager| goes away -- which happens when |browser_state| goes
  // away.
}
