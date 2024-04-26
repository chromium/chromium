// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CONTENT_SUBPROCESS_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CONTENT_SUBPROCESS_METRICS_PROVIDER_H_

#include <map>
#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/scoped_multi_source_observation.h"
#include "base/task/task_runner.h"
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
// TODO(crbug.com/40213327): Do not inherit MetricsProvider.
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

  // Creates the global instance. Returns false if the instance already exists.
  static bool CreateInstance();

  // Returns the global instance.
  static SubprocessMetricsProvider* GetInstance();

  // Merge histograms for all subprocesses. This is used by tests that don't
  // have access to the internal instance of this class.
  static void MergeHistogramDeltasForTesting(
      bool async = false,
      base::OnceClosure done_callback = base::DoNothing());

  // Indicates subprocess to be monitored with unique id for later reference.
  // Metrics reporting will read histograms from it and upload them to UMA.
  void RegisterSubprocessAllocator(
      int id,
      std::unique_ptr<base::PersistentHistogramAllocator> allocator);

  // Indicates that a subprocess has exited and is thus finished with the
  // allocator it was using.
  void DeregisterSubprocessAllocator(int id);

 private:
  friend class SubprocessMetricsProviderTest;

  // Wrapper to add reference counting to an allocator so that it is only
  // released it when all tasks have finished with it. Note that this is
  // RefCounted and not RefCountedThreadSafe, meaning that references should
  // only be created/destroyed on the same sequence (the implementation has
  // DCHECKs to enforce this).
  class RefCountedAllocator : public base::RefCounted<RefCountedAllocator> {
   public:
    explicit RefCountedAllocator(
        std::unique_ptr<base::PersistentHistogramAllocator> allocator);

    RefCountedAllocator(const RefCountedAllocator& other) = delete;
    RefCountedAllocator& operator=(const RefCountedAllocator& other) = delete;

    base::PersistentHistogramAllocator* allocator() { return allocator_.get(); }

   private:
    friend class base::RefCounted<RefCountedAllocator>;
    ~RefCountedAllocator();

    std::unique_ptr<base::PersistentHistogramAllocator> allocator_;
  };

  // The global instance should be accessed through Get().
  SubprocessMetricsProvider();

  // This should never be deleted, as it handles subprocess metrics for the
  // whole lifetime of the browser process.
  ~SubprocessMetricsProvider() override;

  // base::StatisticsRecorder::HistogramProvider:
  void MergeHistogramDeltas(bool async,
                            base::OnceClosure done_callback) override;

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

  // Re-creates |sequenced_task_runner_|. Used for testing.
  void RecreateTaskRunnerForTesting();

  // Merges all histograms of |allocator| to the global StatisticsRecorder. This
  // is called periodically during UMA metrics collection (if enabled) and
  // possibly on-demand for other purposes. May be called on a background
  // thread.
  static void MergeHistogramDeltasFromAllocator(int id,
                                                RefCountedAllocator* allocator);

  // Merges all histograms of the |allocators| to the global StatisticsRecorder.
  // Does not have any form of ownership on the allocators. May be called on a
  // background thread.
  using AllocatorByIdMap = std::map<int, scoped_refptr<RefCountedAllocator>>;
  static void MergeHistogramDeltasFromAllocators(AllocatorByIdMap* allocators);

  // Callback for when MergeHistogramDeltasFromAllocator() is called in a
  // background thread.
  static void OnMergeHistogramDeltasFromAllocator(
      scoped_refptr<RefCountedAllocator> allocator);

  // Callback for when MergeHistogramDeltasFromAllocators() is called in a
  // background thread.
  static void OnMergeHistogramDeltasFromAllocators(
      std::unique_ptr<AllocatorByIdMap> allocators,
      base::OnceClosure done_callback);

  THREAD_CHECKER(thread_checker_);

  // All of the shared-persistent-allocators for known sub-processes.
  AllocatorByIdMap allocators_by_id_;

  // Track all observed render processes to un-observe them on exit.
  // TODO(crbug.com/40213327): Since this class should be leaky, it is not
  // semantically correct to have a "scoped" member field here. Replace this
  // with something like a set.
  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      scoped_observations_{this};

  // Used to asynchronously merge metrics from subprocesses that have exited.
  scoped_refptr<base::TaskRunner> task_runner_;

  base::WeakPtrFactory<SubprocessMetricsProvider> weak_ptr_factory_{this};
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CONTENT_SUBPROCESS_METRICS_PROVIDER_H_
