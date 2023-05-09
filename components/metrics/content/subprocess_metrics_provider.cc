// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/subprocess_metrics_provider.h"

#include <utility>

#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "components/metrics/metrics_features.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/child_process_data.h"

namespace metrics {
namespace {

SubprocessMetricsProvider* g_subprocess_metrics_provider = nullptr;

// Merge all histograms of a given allocator to the global StatisticsRecorder.
// This is called periodically during UMA metrics collection (if enabled) and
// possibly on-demand for other purposes.
void MergeHistogramDeltasFromAllocator(
    int id,
    base::PersistentHistogramAllocator* allocator) {
  DCHECK(allocator);

  int histogram_count = 0;
  base::PersistentHistogramAllocator::Iterator hist_iter(allocator);
  while (true) {
    std::unique_ptr<base::HistogramBase> histogram = hist_iter.GetNext();
    if (!histogram) {
      break;
    }
    allocator->MergeHistogramDeltaToStatisticsRecorder(histogram.get());
    ++histogram_count;
  }

  DVLOG(1) << "Reported " << histogram_count << " histograms from subprocess #"
           << id;
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
void SubprocessMetricsProvider::MergeHistogramDeltasForTesting() {
  GetInstance()->MergeHistogramDeltas();
}

SubprocessMetricsProvider::SubprocessMetricsProvider() {
  base::StatisticsRecorder::RegisterHistogramProvider(
      weak_ptr_factory_.GetWeakPtr());
  content::BrowserChildProcessObserver::Add(this);
  g_subprocess_metrics_provider = this;

  // Ensure no child processes currently exist so that we do not miss any.
  CHECK(content::RenderProcessHost::AllHostsIterator().IsAtEnd());
  CHECK(content::BrowserChildProcessHostIterator().Done());
}

SubprocessMetricsProvider::~SubprocessMetricsProvider() {
  // This should only be called in tests. However, temporarily, this is also
  // called for testing the effects of making this leaky.
  // TODO(crbug/1293026): Eventually ensure this is not called in prod.
  CHECK(
      !base::FeatureList::IsEnabled(features::kSubprocessMetricsProviderLeaky));

  // Safe even if this object has never been added as an observer.
  content::BrowserChildProcessObserver::Remove(this);
  g_subprocess_metrics_provider = nullptr;
}

void SubprocessMetricsProvider::RegisterSubprocessAllocator(
    int id,
    std::unique_ptr<base::PersistentHistogramAllocator> allocator) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(allocator);

  // Insert the allocator into the internal map and verify that there was no
  // allocator with the same ID already.
  auto result = allocators_by_id_.emplace(id, std::move(allocator));
  CHECK(result.second);
}

void SubprocessMetricsProvider::DeregisterSubprocessAllocator(int id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = allocators_by_id_.find(id);
  if (it == allocators_by_id_.end()) {
    return;
  }

  // Extract the matching allocator from the list of active ones. It will be
  // automatically released when this method exits.
  std::unique_ptr<base::PersistentHistogramAllocator> allocator =
      std::move(it->second);
  allocators_by_id_.erase(it);
  CHECK(allocator);

  // Merge the last deltas from the allocator before it is released.
  MergeHistogramDeltasFromAllocator(id, allocator.get());
}

void SubprocessMetricsProvider::MergeHistogramDeltas() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& iter : allocators_by_id_) {
    MergeHistogramDeltasFromAllocator(iter.first, iter.second.get());
  }
}

void SubprocessMetricsProvider::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // See if the new process has a memory allocator and take control of it if so.
  // This call can only be made on the browser's IO thread.
  content::BrowserChildProcessHost* host =
      content::BrowserChildProcessHost::FromID(data.id);
  CHECK(host);

  std::unique_ptr<base::PersistentMemoryAllocator> allocator =
      host->TakeMetricsAllocator();
  // The allocator can be null in tests.
  if (!allocator)
    return;

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
  if (!scoped_observations_.IsObservingSource(host))
    scoped_observations_.AddObservation(host);
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

}  // namespace metrics
