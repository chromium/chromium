// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/subprocess_metrics_provider.h"

#include <utility>

#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/metrics/metrics_features.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/child_process_data.h"

namespace metrics {
namespace {

SubprocessMetricsProvider* g_subprocess_metrics_provider = nullptr;

scoped_refptr<base::TaskRunner> CreateTaskRunner() {
  // This task runner must block shutdown to ensure metrics are not lost.
  return base::ThreadPool::CreateTaskRunner(
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
}

}  // namespace

// static
bool SubprocessMetricsProvider::CreateInstance() {
  if (g_subprocess_metrics_provider) {
    return false;
  }
  g_subprocess_metrics_provider = new SubprocessMetricsProvider();
  ANNOTATE_LEAKING_OBJECT_PTR(g_subprocess_metrics_provider);
  return true;
}

// static
SubprocessMetricsProvider* SubprocessMetricsProvider::GetInstance() {
  return g_subprocess_metrics_provider;
}

// static
void SubprocessMetricsProvider::MergeHistogramDeltasForTesting(
    bool async,
    base::OnceClosure done_callback) {
  GetInstance()->MergeHistogramDeltas(async, std::move(done_callback));
}

SubprocessMetricsProvider::RefCountedAllocator::RefCountedAllocator(
    std::unique_ptr<base::PersistentHistogramAllocator> allocator)
    : allocator_(std::move(allocator)) {
  CHECK(allocator_);
}

SubprocessMetricsProvider::RefCountedAllocator::~RefCountedAllocator() =
    default;

SubprocessMetricsProvider::SubprocessMetricsProvider()
    : task_runner_(CreateTaskRunner()) {
  base::StatisticsRecorder::RegisterHistogramProvider(
      weak_ptr_factory_.GetWeakPtr());
  content::BrowserChildProcessObserver::Add(this);
  g_subprocess_metrics_provider = this;

  // Ensure no child processes currently exist so that we do not miss any.
  CHECK(content::RenderProcessHost::AllHostsIterator().IsAtEnd());
  CHECK(content::BrowserChildProcessHostIterator().Done());
}

SubprocessMetricsProvider::~SubprocessMetricsProvider() {
  // This object should never be deleted since it is leaky.
  NOTREACHED_IN_MIGRATION();
}

void SubprocessMetricsProvider::RegisterSubprocessAllocator(
    int id,
    std::unique_ptr<base::PersistentHistogramAllocator> allocator) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(allocator);

  // Pass a custom RangesManager so that we do not register the BucketRanges
  // with the global StatisticsRecorder when creating histogram objects using
  // the allocator's underlying data. This avoids unnecessary contention on the
  // global StatisticsRecorder lock.
  // Note: Since |allocator| may be merged from different threads concurrently,
  // for example on the UI thread and on a background thread, we must use
  // ThreadSafeRangesManager.
  allocator->SetRangesManager(new base::ThreadSafeRangesManager());
  // Insert the allocator into the internal map and verify that there was no
  // allocator with the same ID already.
  auto result = allocators_by_id_.emplace(
      id, base::MakeRefCounted<RefCountedAllocator>(std::move(allocator)));
  CHECK(result.second);
}

void SubprocessMetricsProvider::DeregisterSubprocessAllocator(int id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = allocators_by_id_.find(id);
  if (it == allocators_by_id_.end()) {
    return;
  }

  // Extract the matching allocator from the list of active ones. It is possible
  // that a background task is currently holding a reference to it. Removing it
  // from the internal map is fine though, as it is refcounted.
  scoped_refptr<RefCountedAllocator> allocator = std::move(it->second);
  allocators_by_id_.erase(it);
  CHECK(allocator);

  // Merge the last deltas from the allocator before releasing the ref (and
  // deleting if the last one).
  auto* allocator_ptr = allocator.get();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &SubprocessMetricsProvider::MergeHistogramDeltasFromAllocator, id,
          // Unretained is needed to pass a refcounted class as a raw pointer.
          // It is safe because it is kept alive by the reply task.
          base::Unretained(allocator_ptr)),
      base::BindOnce(
          &SubprocessMetricsProvider::OnMergeHistogramDeltasFromAllocator,
          std::move(allocator)));
}

void SubprocessMetricsProvider::MergeHistogramDeltas(
    bool async,
    base::OnceClosure done_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (async) {
    // Make a copy of the internal allocators map (with its own refptrs) to pass
    // to the background task.
    auto allocators = std::make_unique<AllocatorByIdMap>(allocators_by_id_);
    auto* allocators_ptr = allocators.get();

    // This is intentionally not posted to |task_runner_| because not running
    // this task does not imply data loss, so no point in blocking shutdown
    // (hence CONTINUE_ON_SHUTDOWN). However, there might be some contention on
    // the StatisticsRecorder between this task and those posted to
    // |task_runner_|.
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&MergeHistogramDeltasFromAllocators, allocators_ptr),
        base::BindOnce(
            &SubprocessMetricsProvider::OnMergeHistogramDeltasFromAllocators,
            std::move(allocators), std::move(done_callback)));
  } else {
    MergeHistogramDeltasFromAllocators(&allocators_by_id_);
    std::move(done_callback).Run();
  }
}

void SubprocessMetricsProvider::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // See if the new process has a memory allocator and take control of it if so.
  // This call can only be made on the browser's IO thread.
  content::BrowserChildProcessHost* host =
      content::BrowserChildProcessHost::FromID(data.id);
  // |host| should not be null, but such cases have been observed in the wild so
  // gracefully handle this scenario.
  if (!host) {
    return;
  }

  std::unique_ptr<base::PersistentMemoryAllocator> allocator =
      host->TakeMetricsAllocator();
  // The allocator can be null in tests.
  if (!allocator) {
    return;
  }

  RegisterSubprocessAllocator(
      data.id, std::make_unique<base::PersistentHistogramAllocator>(
                   std::move(allocator)));
}

void SubprocessMetricsProvider::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DeregisterSubprocessAllocator(data.id);
}

void SubprocessMetricsProvider::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DeregisterSubprocessAllocator(data.id);
}

void SubprocessMetricsProvider::BrowserChildProcessKilled(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DeregisterSubprocessAllocator(data.id);
}

void SubprocessMetricsProvider::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
  // Sometimes, the same host will cause multiple notifications in tests so
  // could possibly do the same in a release build.
  if (!scoped_observations_.IsObservingSource(host)) {
    scoped_observations_.AddObservation(host);
  }
}

void SubprocessMetricsProvider::RenderProcessReady(
    content::RenderProcessHost* host) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If the render-process-host passed a persistent-memory-allocator to the
  // renderer process, extract it and register it here.
  std::unique_ptr<base::PersistentMemoryAllocator> allocator =
      host->TakeMetricsAllocator();
  if (allocator) {
    RegisterSubprocessAllocator(
        host->GetID(), std::make_unique<base::PersistentHistogramAllocator>(
                           std::move(allocator)));
  }
}

void SubprocessMetricsProvider::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DeregisterSubprocessAllocator(host->GetID());
}

void SubprocessMetricsProvider::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // It's possible for a Renderer to terminate without RenderProcessExited
  // (above) being called so it's necessary to de-register also upon the
  // destruction of the host. If both get called, no harm is done.

  DeregisterSubprocessAllocator(host->GetID());
  scoped_observations_.RemoveObservation(host);
}

void SubprocessMetricsProvider::RecreateTaskRunnerForTesting() {
  task_runner_ = CreateTaskRunner();
}

// static
void SubprocessMetricsProvider::MergeHistogramDeltasFromAllocator(
    int id,
    RefCountedAllocator* allocator) {
  DCHECK(allocator);

  int histogram_count = 0;
  base::PersistentHistogramAllocator* allocator_ptr = allocator->allocator();
  base::PersistentHistogramAllocator::Iterator hist_iter(allocator_ptr);
  while (true) {
    std::unique_ptr<base::HistogramBase> histogram = hist_iter.GetNext();
    if (!histogram) {
      break;
    }
    // We expect histograms to match as subprocesses shouldn't have version skew
    // with the browser process.
    bool merge_success =
        allocator_ptr->MergeHistogramDeltaToStatisticsRecorder(histogram.get());

    // When merging child process histograms into the parent, we expect the
    // merge operation to succeed. If it doesn't, it means the histograms have
    // different types or buckets, which indicates a programming error (i.e.
    // non-matching logging code across browser and child for a histogram). In
    // this case DumpWithoutCrashing() with a crash key with the histogram name.
    if (!merge_success) {
      SCOPED_CRASH_KEY_STRING256("SubprocessMetricsProvider", "histogram",
                                 histogram->histogram_name());
      base::debug::DumpWithoutCrashing();
    }
    ++histogram_count;
  }

  DVLOG(1) << "Reported " << histogram_count << " histograms from subprocess #"
           << id;
}

// static
void SubprocessMetricsProvider::MergeHistogramDeltasFromAllocators(
    AllocatorByIdMap* allocators) {
  for (const auto& iter : *allocators) {
    MergeHistogramDeltasFromAllocator(iter.first, iter.second.get());
  }
}

// static
void SubprocessMetricsProvider::OnMergeHistogramDeltasFromAllocator(
    scoped_refptr<RefCountedAllocator> allocator) {
  // This method does nothing except have ownership on |allocator|. When this
  // method exits, |allocator| will be released (unless there are background
  // tasks currently holding references).
}

// static
void SubprocessMetricsProvider::OnMergeHistogramDeltasFromAllocators(
    std::unique_ptr<AllocatorByIdMap> allocators,
    base::OnceClosure done_callback) {
  std::move(done_callback).Run();
  // When this method exits, |allocators| will be released. It's possible some
  // allocators are from subprocesses that have already been deregistered, so
  // they will also be released here (assuming no other background tasks
  // currently hold references).
}

}  // namespace metrics
