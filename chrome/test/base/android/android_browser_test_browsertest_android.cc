// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"
#include "chrome/browser/preloading/preloading_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/background_task_scheduler/background_task_scheduler.h"
#include "components/background_task_scheduler/background_task_scheduler_factory.h"
#include "components/background_task_scheduler/task_ids.h"
#include "components/background_task_scheduler/task_info.h"
#include "components/performance_manager/public/features.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/test/test_network_connection_tracker.h"

IN_PROC_BROWSER_TEST_F(AndroidBrowserTest, Smoke) {
  size_t model_count = 0;
  const TabModel* tab_model = nullptr;
  TabModelList::ForEachAll([&](TabModel* model) {
    if (!tab_model) {
      tab_model = model;
    }
    ++model_count;
    return true;
  });
  ASSERT_EQ(model_count, 1u);

  // Grab a tab an navigate its contents
  EXPECT_TRUE(content::NavigateToURL(tab_model->GetActiveWebContents(),
                                     GURL("about:blank")));
}

// Verifies that state is preserved between PRE_ stages on Android.
// Historically PRE_ was not supported, which is why there's an explicit test
// for this behavior.
IN_PROC_BROWSER_TEST_F(AndroidBrowserTest, PRE_PRE_StatePreserved) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  Profile* profile = GetProfile();
  ASSERT_FALSE(base::DirectoryExists(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_PRE_DIR"))));
  ASSERT_FALSE(base::DirectoryExists(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_DIR"))));

  ASSERT_TRUE(base::CreateDirectory(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_PRE_DIR"))));
  ASSERT_TRUE(base::DirectoryExists(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_PRE_DIR"))));
}

IN_PROC_BROWSER_TEST_F(AndroidBrowserTest, PRE_StatePreserved) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  Profile* profile = GetProfile();
  ASSERT_TRUE(base::DirectoryExists(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_PRE_DIR"))));
  ASSERT_FALSE(base::DirectoryExists(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_DIR"))));

  ASSERT_TRUE(base::CreateDirectory(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_DIR"))));
}

IN_PROC_BROWSER_TEST_F(AndroidBrowserTest, StatePreserved) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  Profile* profile = GetProfile();
  ASSERT_TRUE(base::DirectoryExists(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_PRE_DIR"))));
  ASSERT_TRUE(base::DirectoryExists(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_DIR"))));
}

IN_PROC_BROWSER_TEST_F(AndroidBrowserTest, NetworkIsReportedOnline) {
  auto type = net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE;
  const bool answered_synchronously =
      content::GetNetworkConnectionTracker()->GetConnectionType(
          &type, base::DoNothing());

  EXPECT_TRUE(answered_synchronously);
  EXPECT_NE(net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE, type);
}

class AndroidBrowserTestWithRealNetworkConnectionTracker
    : public AndroidBrowserTest {
 public:
  AndroidBrowserTestWithRealNetworkConnectionTracker() {
    set_use_test_network_connection_tracker(false);
  }
};

IN_PROC_BROWSER_TEST_F(AndroidBrowserTestWithRealNetworkConnectionTracker,
                       UsesRealTracker) {
  EXPECT_FALSE(network::TestNetworkConnectionTracker::HasInstance());
}

IN_PROC_BROWSER_TEST_F(AndroidBrowserTest, DefaultStartupFeatures) {
  EXPECT_FALSE(base::FeatureList::IsEnabled(features::kPreconnectToSearch));
  EXPECT_FALSE(base::FeatureList::IsEnabled(
      features::kRestartNetworkServiceUnsandboxedForFailedLaunch));
  EXPECT_FALSE(base::FeatureList::IsEnabled(
      performance_manager::features::kTransientKeepAlivePolicy));
  EXPECT_FALSE(base::FeatureList::IsEnabled(features::kPrewarm));
}

// Verifies that `ChromeBrowserTestsApplication` installs a no-op
// `BackgroundTaskScheduler` by default so background tasks scheduled during
// browser tests succeed without queuing jobs in Android's `JobScheduler`.
IN_PROC_BROWSER_TEST_F(AndroidBrowserTest, DefaultNoOpBackgroundTaskScheduler) {
  base::HistogramTester histogram_tester;
  background_task::OneOffInfo one_off_info;
  one_off_info.window_start_time_ms = 0;
  one_off_info.window_end_time_ms = 0;
  background_task::TaskInfo task_info(
      background_task::TaskIds::SAFETY_HUB_JOB_ID, one_off_info);

  background_task::BackgroundTaskScheduler* scheduler =
      background_task::BackgroundTaskSchedulerFactory::GetScheduler();
  EXPECT_TRUE(scheduler->Schedule(task_info));
  scheduler->Cancel(background_task::TaskIds::SAFETY_HUB_JOB_ID);

  // `BackgroundTaskSchedulerImpl` records these histograms before delegating to
  // `JobScheduler`; zero samples confirms the default no-op scheduler is
  // active.
  histogram_tester.ExpectTotalCount(
      "Android.BackgroundTaskScheduler.TaskScheduled.Success", 0);
  histogram_tester.ExpectTotalCount(
      "Android.BackgroundTaskScheduler.TaskCanceled", 0);
}
