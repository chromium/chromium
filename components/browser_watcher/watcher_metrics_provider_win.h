// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_WATCHER_WATCHER_METRICS_PROVIDER_WIN_H_
#define COMPONENTS_BROWSER_WATCHER_WATCHER_METRICS_PROVIDER_WIN_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/task_runner.h"
#include "components/metrics/metrics_provider.h"

namespace browser_watcher {

// Provides stability data captured by the Chrome Watcher, namely the browser
// process exit codes.
class WatcherMetricsProviderWin : public metrics::MetricsProvider {
 public:
  // A callback that provides product name, version number and channel name.
  using GetExecutableDetailsCallback =
      base::Callback<void(base::string16*, base::string16*, base::string16*)>;

  static const char kBrowserExitCodeHistogramName[];

  // Initializes the reporter.
  WatcherMetricsProviderWin(const base::string16& registry_path,
                            const base::FilePath& user_data_dir,
                            const base::FilePath& crash_dir,
                            const GetExecutableDetailsCallback& exe_details_cb);
  ~WatcherMetricsProviderWin() override;

  // metrics::MetricsProvider implementation.
  void AsyncInit(const base::Closure& done_callback) override;
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  // Note: this function collects metrics, some of which are related to the
  // previous run's version and some to the current version. Doing the correct
  // attribution on upgrade is difficult, and currently ignored. Metrics
  // clearing is one mechanism to avoid misattribution, but is not used in this
  // case (ClearSavedStabilityMetrics is not overridden) as version
  // misattribution is preferred to data loss. Metrics will likely be attributed
  // to the previous run's version, unless no initial log is sent, in which case
  // they should be attributed to the current version (though they may actually
  // be attributed to still another following version).
  // TODO(manzagop): proper metric version attribution on upgrade.
  void ProvideStabilityMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;

 private:
  // TODO(manzagop): avoid collecting reports for clean exits from the fast exit
  // path.
  void CollectPostmortemReportsImpl();

  bool recording_enabled_;
  bool cleanup_scheduled_;
  const base::string16 registry_path_;
  const base::FilePath user_data_dir_;
  const base::FilePath crash_dir_;
  GetExecutableDetailsCallback exe_details_cb_;

  // Used for collecting postmortem reports and clearing leftover data in
  // registry if metrics reporting is disabled.
  scoped_refptr<base::TaskRunner> task_runner_;

  base::WeakPtrFactory<WatcherMetricsProviderWin> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WatcherMetricsProviderWin);
};

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_WATCHER_METRICS_PROVIDER_WIN_H_
