// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LIST_LAUNCH_METRICS_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LIST_LAUNCH_METRICS_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_list_launch_recorder.h"
#include "components/metrics/metrics_provider.h"
#include "third_party/metrics_proto/chrome_os_app_list_launch_event.pb.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

class ChromeMetricsServiceClient;

namespace app_list {

class AppListLaunchMetricsProviderTest;

// AppListLaunchMetricsProvider is responsible for filling out the
// |app_list_launch_event| section of the UMA proto. This class should not be
// instantiated directly except by the ChromeMetricsServiceClient. Instead,
// logging events should be sent to AppListLaunchRecorder.
class AppListLaunchMetricsProvider : public metrics::MetricsProvider {
 public:
  ~AppListLaunchMetricsProvider() override;

  // metrics::MetricsProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  friend class ::ChromeMetricsServiceClient;
  friend class app_list::AppListLaunchMetricsProviderTest;

  // Beyond this number of OnAppListLaunch calls between successive calls to
  // ProvideCurrentSessionData, we stop logging.
  static int kMaxEventsPerUpload;

  // The constructors are private so that this class can only be instantied by
  // ChromeMetricsServiceClient and tests.
  AppListLaunchMetricsProvider();

  // Records the information in |launch_info|, to be converted into a hashed
  // form and logged to UMA. Actual logging occurs periodically, so it is not
  // guaranteed that any call to this method will be logged.
  //
  // OnAppListLaunch should only be called from the browser UI sequence.
  void OnAppListLaunch(const AppListLaunchRecorder::LaunchInfo& launch_info);

  // Performs initialization once a user has logged in.
  void Initialize();

  // Subscription for receiving logging event callbacks from HashedLogger.
  std::unique_ptr<AppListLaunchRecorder::LaunchEventSubscription> subscription_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AppListLaunchMetricsProvider> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppListLaunchMetricsProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LIST_LAUNCH_METRICS_PROVIDER_H_
