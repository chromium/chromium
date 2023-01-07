// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/uninstall_view.h"

#include "base/functional/callback_helpers.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

using UninstallViewTest = ChromeViewsTestBase;

TEST_F(UninstallViewTest, Accept) {
  int result = -1;
  UninstallView view(&result, base::DoNothing());
  view.Accept();
  EXPECT_EQ(result, content::RESULT_CODE_NORMAL_EXIT);
}

TEST_F(UninstallViewTest, Cancel) {
  int result = -1;
  UninstallView view(&result, base::DoNothing());
  view.Cancel();
  EXPECT_EQ(result, chrome::RESULT_CODE_UNINSTALL_USER_CANCEL);
}

TEST_F(UninstallViewTest, Close) {
  int result = -1;
  UninstallView view(&result, base::DoNothing());
  view.Close();
  EXPECT_EQ(result, chrome::RESULT_CODE_UNINSTALL_USER_CANCEL);
}
