// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CONTENT_SUBPROCESS_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CONTENT_SUBPROCESS_METRICS_PROVIDER_H_

#include <map>
#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/scoped_multi_source_observation.h"
#include "base/threading/thread_checker.h"
#include "components/metrics/metrics_provider.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"

namespace base {
class PersistentHistogramAllocator;
}

namespace metrics {

// SubprocessMetricsProvider gathers and merges histograms stored in shared
// memory segments between processes. Merging occurs when a process exits,
// when metrics are being collected for upload, or when something else needs
// combined metrics (such as the chrome://histograms page).
// TODO(crbug/1293026): Do not inherit MetricsProvider.
class SubprocessMetricsProvider
    : public MetricsProvider,
      public base::StatisticsRecorder::HistogramProvider,
      public content::BrowserChildProcessObserver,
      public content::RenderProcessHostCreationObserver,
      public content::RenderProcessHostObserver {
 public:
  SubprocessMetricsProvider(const SubprocessMetricsProvider&) = delete;
  SubprocessMetricsProvider& operator=(const SubprocessMetricsProvider&) =
      delete;

  // This should never be deleted, as it handles subprocess metrics for the
  // whole lifetime of the browser process. However, it may be deleted in tests.
  // Also temporarily while testing the effects of making this leaky.
  // TODO(crbug/1293026): Make this private.
  ~SubprocessMetricsProvider() override;

  // Creates the global instance. Returns false if the instance already exists.
  static bool CreateInstance();

  // Returns the global instance.
  static SubprocessMetricsProvider* GetInstance();

  // Merge histograms for all subprocesses. This is used by tests that don't
  // have access to the internal instance of this class.
  static void MergeHistogramDeltasForTesting();

 private:
  friend class SubprocessMetricsProviderTest;
  friend class SubprocessMetricsProviderBrowserTest;

  // The global instance should be accessed through Get().
  SubprocessMetricsProvider();

  // Indicates subprocess to be monitored with unique id for later reference.
  // Metrics reporting will read histograms from it and upload them to UMA.
  void RegisterSubprocessAllocator(
      int id,
      std::unique_ptr<base::PersistentHistogramAllocator> allocator);

  // Indicates that a subprocess has exited and is thus finished with the
  // allocator it was using.
  void DeregisterSubprocessAllocator(int id);

  // base::StatisticsRecorder::HistogramProvider:
  void MergeHistogramDeltas() override;

  // content::BrowserChildProcessObserver:
  void BrowserChildProcessLaunchedAndConnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) override;
  void BrowserChildProcessCrashed(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;
  void BrowserChildProcessKilled(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override;

  // content::RenderProcessHostObserver:
  void RenderProcessReady(content::RenderProcessHost* host) override;
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  THREAD_CHECKER(thread_checker_);

  // All of the shared-persistent-allocators for known sub-processes.
  using AllocatorByIdMap =
      std::map<int, std::unique_ptr<base::PersistentHistogramAllocator>>;
  AllocatorByIdMap allocators_by_id_;

  // Track all observed render processes to un-observe them on exit.
  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      scoped_observations_{this};

  base::WeakPtrFactory<SubprocessMetricsProvider> weak_ptr_factory_{this};
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CONTENT_SUBPROCESS_METRICS_PROVIDER_H_
