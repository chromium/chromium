// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/window_creation.h"

#include "base/metrics/statistics_recorder.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#endif  // !BUILDFLAG(IS_MAC)

namespace waap {

using WaapWindowCreationBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(WaapWindowCreationBrowserTest,
                       CreateBrowserWindowAndRecordCreationSource) {
  const base::TimeTicks trigger_time =
      base::TimeTicks::Now() - base::Milliseconds(25);
  BrowserWindowInterface* new_browser =
      CreateBrowserWindowAndRecordCreationSource(
          BrowserWindowCreateParams(*browser()->GetProfile(),
                                    /*from_user_gesture=*/false),
          NewWindowCreationSource::kNewEmptyWindow, trigger_time);

  ASSERT_TRUE(new_browser);
  ASSERT_NE(new_browser, browser());
  EXPECT_EQ(new_browser->GetProfile(), browser()->GetProfile());
  EXPECT_EQ(BrowserWindowInterface::TYPE_NORMAL, new_browser->GetType());
  auto* manager = InitialWebUIWindowMetricsManager::From(new_browser);
  ASSERT_TRUE(manager);
  EXPECT_EQ(NewWindowCreationSource::kNewEmptyWindow,
            manager->GetCreationSourceForTesting());
  EXPECT_EQ(trigger_time, manager->GetTriggerTimeForTesting());
}

#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(
    WaapWindowCreationBrowserTest,
    AcceleratorPressedWithTimestampRecordsFromTriggerMetric) {
  constexpr char kFromTrigger[] =
      "InitialWebUI.NewWindow.NewEmptyWindow.WithExistingWindow."
      "BrowserWindow.FirstPaint.FromTrigger3";
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(browser_view);
  base::HistogramTester histogram_tester;
  base::StatisticsRecorder::HistogramWaiter waiter(kFromTrigger);
  // Records new-window metrics even if `browser()` has not painted.
  WaapUIMetricsService::SkipStartupForTesting();
  ui_test_utils::BrowserCreatedObserver browser_created_observer;

  const base::TimeTicks trigger_time =
      base::TimeTicks::Now() - base::Milliseconds(25);
  EXPECT_TRUE(browser_view->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_N, ui::EF_PLATFORM_ACCELERATOR,
                      ui::Accelerator::KeyState::PRESSED, trigger_time)));

  BrowserWindowInterface* new_browser = browser_created_observer.Wait();
  ASSERT_TRUE(new_browser);
  auto* manager = InitialWebUIWindowMetricsManager::From(new_browser);
  ASSERT_TRUE(manager);
  EXPECT_EQ(trigger_time, manager->GetTriggerTimeForTesting());
  waiter.Wait();
  histogram_tester.ExpectTotalCount(kFromTrigger, 1);
}

IN_PROC_BROWSER_TEST_F(WaapWindowCreationBrowserTest,
                       NewIncognitoWindowAcceleratorSetsTriggerTime) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(browser_view);
  ui_test_utils::BrowserCreatedObserver browser_created_observer;

  const base::TimeTicks trigger_time =
      base::TimeTicks::Now() - base::Milliseconds(25);
  EXPECT_TRUE(browser_view->AcceleratorPressed(ui::Accelerator(
      ui::VKEY_N, ui::EF_PLATFORM_ACCELERATOR | ui::EF_SHIFT_DOWN,
      ui::Accelerator::KeyState::PRESSED, trigger_time)));

  BrowserWindowInterface* new_browser = browser_created_observer.Wait();
  ASSERT_TRUE(new_browser);
  EXPECT_TRUE(new_browser->GetProfile()->IsIncognitoProfile());
  auto* manager = InitialWebUIWindowMetricsManager::From(new_browser);
  ASSERT_TRUE(manager);
  EXPECT_EQ(trigger_time, manager->GetTriggerTimeForTesting());
}
#endif  // !BUILDFLAG(IS_MAC)

// Commands run without an input event, such as from the app menu, carry no
// event time.
IN_PROC_BROWSER_TEST_F(WaapWindowCreationBrowserTest,
                       CommandWithoutInputEventHasNoTriggerTime) {
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_NEW_WINDOW));

  BrowserWindowInterface* new_browser = browser_created_observer.Wait();
  ASSERT_TRUE(new_browser);
  auto* manager = InitialWebUIWindowMetricsManager::From(new_browser);
  ASSERT_TRUE(manager);
  EXPECT_TRUE(manager->GetTriggerTimeForTesting().is_null());
}

IN_PROC_BROWSER_TEST_F(WaapWindowCreationBrowserTest,
                       CreateBrowserWindowAsyncAndRecordCreationSource) {
  base::test::TestFuture<BrowserWindowInterface*> future;
  CreateBrowserWindowAndRecordCreationSource(
      BrowserWindowCreateParams(*browser()->GetProfile(),
                                /*from_user_gesture=*/false),
      NewWindowCreationSource::kTest, future.GetCallback());
  BrowserWindowInterface* new_browser = future.Get();

  ASSERT_TRUE(new_browser);
  ASSERT_NE(new_browser, browser());
  EXPECT_EQ(new_browser->GetProfile(), browser()->GetProfile());
  EXPECT_EQ(BrowserWindowInterface::TYPE_NORMAL, new_browser->GetType());
  auto* manager = InitialWebUIWindowMetricsManager::From(new_browser);
  ASSERT_TRUE(manager);
  EXPECT_EQ(NewWindowCreationSource::kTest,
            manager->GetCreationSourceForTesting());
}

}  // namespace waap
