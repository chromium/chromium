// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_client_impl.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"

// Interactive UI Test for AppListClientImpl that runs on all platforms
// supporting app_list. Interactive because the app list uses focus changes to
// dismiss itself, which will cause tests that check the visibility to fail
// flakily.
using AppListClientInteractiveTest = InProcessBrowserTest;

// Show the app list, then dismiss it.
IN_PROC_BROWSER_TEST_F(AppListClientInteractiveTest, ShowAndDismiss) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_FALSE(client->app_list_visible());
  client->ShowAppList();
  ASSERT_TRUE(client->app_list_visible());
  client->DismissView();
  ASSERT_FALSE(client->app_list_target_visibility());
}
