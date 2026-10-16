// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/initial_webui_metrics_dispatcher.h"

#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace {

class InitialWebUIMetricsDispatcherTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    EXPECT_CALL(browser_window_, GetProfile())
        .WillRepeatedly(testing::Return(profile()));
    EXPECT_CALL(testing::Const(browser_window_), GetProfile())
        .WillRepeatedly(testing::Return(profile()));
    EXPECT_CALL(browser_window_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
  }

  MockBrowserWindowInterface browser_window_;
  ui::UnownedUserDataHost unowned_user_data_host_;
};

TEST_F(InitialWebUIMetricsDispatcherTest, WebContentsDispatcherCreation) {
  EXPECT_EQ(
      InitialWebUIWebContentsMetricsDispatcher::FromWebContents(web_contents()),
      nullptr);

  auto* dispatcher =
      InitialWebUIMetricsDispatcher::GetOrCreateForWebContents(web_contents());
  ASSERT_NE(dispatcher, nullptr);
  EXPECT_EQ(
      InitialWebUIWebContentsMetricsDispatcher::FromWebContents(web_contents()),
      dispatcher);

  base::TimeTicks now = base::TimeTicks::Now();
  dispatcher->OnCreated();
  dispatcher->OnCommit(now, now);
  dispatcher->OnFirstPaint(now);
  dispatcher->OnFirstContentfulPaint(now);
}

TEST_F(InitialWebUIMetricsDispatcherTest, NativeDispatcherCreation) {
  InitialWebUIWindowMetricsManager manager(&browser_window_);
  InitialWebUINativeMetricsDispatcher dispatcher(profile(), &manager);

  base::TimeTicks now = base::TimeTicks::Now();
  dispatcher.OnCreated();
  dispatcher.OnCommit(now, now);
  dispatcher.OnFirstPaint(now);
  dispatcher.OnFirstContentfulPaint(now);
}

}  // namespace
