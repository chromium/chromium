// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CONTENT_CONTENT_STABILITY_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CONTENT_CONTENT_STABILITY_METRICS_PROVIDER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/stability_metrics_helper.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

#if defined(OS_ANDROID)
#include "components/crash/content/browser/crash_metrics_reporter_android.h"
#endif  // defined(OS_ANDROID)

class PrefService;

namespace metrics {

class ExtensionsHelper;

// ContentStabilityMetricsProvider gathers and logs Chrome-specific stability-
// related metrics.
class ContentStabilityMetricsProvider
    : public MetricsProvider,
      public content::BrowserChildProcessObserver,
#if defined(OS_ANDROID)
      public crash_reporter::CrashMetricsReporter::Observer,
#endif
      public content::NotificationObserver {
 public:
  // |extensions_helper| is used to determine if a process corresponds to an
  // extension and is optional. If an ExtensionsHelper is not supplied it is
  // assumed the process does not correspond to an extension.
  ContentStabilityMetricsProvider(
      PrefService* local_state,
      std::unique_ptr<ExtensionsHelper> extensions_helper);
  ContentStabilityMetricsProvider(const ContentStabilityMetricsProvider&) =
      delete;
  ContentStabilityMetricsProvider& operator=(
      const ContentStabilityMetricsProvider&) = delete;
  ~ContentStabilityMetricsProvider() override;

  // MetricsDataProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideStabilityMetrics(
      SystemProfileProto* system_profile_proto) override;
  void ClearSavedStabilityMetrics() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ContentStabilityMetricsProviderTest,
                           BrowserChildProcessObserverGpu);
  FRIEND_TEST_ALL_PREFIXES(ContentStabilityMetricsProviderTest,
                           BrowserChildProcessObserverUtility);
  FRIEND_TEST_ALL_PREFIXES(ContentStabilityMetricsProviderTest,
                           NotificationObserver);
  FRIEND_TEST_ALL_PREFIXES(ContentStabilityMetricsProviderTest,
                           ExtensionsNotificationObserver);

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // content::BrowserChildProcessObserver:
  void BrowserChildProcessCrashed(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;
  void BrowserChildProcessLaunchedAndConnected(
      const content::ChildProcessData& data) override;

#if defined(OS_ANDROID)
  // crash_reporter::CrashMetricsReporter::Observer:
  void OnCrashDumpProcessed(
      int rph_id,
      const crash_reporter::CrashMetricsReporter::ReportedCrashTypeSet&
          reported_counts) override;

  ScopedObserver<crash_reporter::CrashMetricsReporter,
                 crash_reporter::CrashMetricsReporter::Observer>
      scoped_observer_;
#endif  // defined(OS_ANDROID)

  StabilityMetricsHelper helper_;

  // Registrar for receiving stability-related notifications.
  content::NotificationRegistrar registrar_;

  std::unique_ptr<ExtensionsHelper> extensions_helper_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CONTENT_CONTENT_STABILITY_METRICS_PROVIDER_H_
