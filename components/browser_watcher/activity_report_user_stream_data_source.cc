// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/activity_report_user_stream_data_source.h"

#include <string>
#include <utility>

#include <windows.h>

// Must be included after windows.h.
#include <psapi.h>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/memory/free_deleter.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/process/memory.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/browser_watcher/activity_report_extractor.h"
#include "components/browser_watcher/activity_tracker_annotation.h"
#include "components/browser_watcher/extended_crash_reporting_metrics.h"
#include "components/browser_watcher/minidump_user_streams.h"
#include "third_party/crashpad/crashpad/minidump/minidump_user_extension_stream_data_source.h"
#include "third_party/crashpad/crashpad/snapshot/annotation_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/exception_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/module_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/process_snapshot.h"
#include "third_party/crashpad/crashpad/util/process/process_memory.h"

namespace browser_watcher {

namespace {

// TODO(siggi): Refactor this to harmonize with the activity tracker setup.
const size_t kMaxActivityAnnotationSize = 2 << 20;

using UniqueMallocPtr = std::unique_ptr<void, base::FreeDeleter>;

UniqueMallocPtr UncheckedAllocate(size_t size) {
  void* raw_ptr = nullptr;
  if (!base::UncheckedMalloc(size, &raw_ptr))
    return UniqueMallocPtr();

  return UniqueMallocPtr(raw_ptr);
}

// A PersistentMemoryAllocator subclass that can take ownership of a buffer
// that's allocated with a malloc-compatible allocation function.
class MallocMemoryAllocator : public base::PersistentMemoryAllocator {
 public:
  MallocMemoryAllocator(UniqueMallocPtr buffer, size_t size);
  ~MallocMemoryAllocator() override;
};

MallocMemoryAllocator::MallocMemoryAllocator(UniqueMallocPtr buffer,
                                             size_t size)
    : base::PersistentMemoryAllocator(buffer.release(), size, 0, 0, "", true) {}

MallocMemoryAllocator::~MallocMemoryAllocator() {
  free(const_cast<char*>(mem_base_));
}

class BufferExtensionStreamDataSource final
    : public crashpad::MinidumpUserExtensionStreamDataSource {
 public:
  explicit BufferExtensionStreamDataSource(uint32_t stream_type);

  bool Init(const StabilityReport& report);

  size_t StreamDataSize() override;
  bool ReadStreamData(Delegate* delegate) override;

 private:
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(BufferExtensionStreamDataSource);
};

BufferExtensionStreamDataSource::BufferExtensionStreamDataSource(
    uint32_t stream_type)
    : crashpad::MinidumpUserExtensionStreamDataSource(stream_type) {}

bool BufferExtensionStreamDataSource::Init(const StabilityReport& report) {
  if (report.SerializeToString(&data_))
    return true;
  data_.clear();
  return false;
}

size_t BufferExtensionStreamDataSource::StreamDataSize() {
  DCHECK(!data_.empty());
  return data_.size();
}

bool BufferExtensionStreamDataSource::ReadStreamData(Delegate* delegate) {
  DCHECK(!data_.empty());
  return delegate->ExtensionStreamDataSourceRead(
      data_.size() ? data_.data() : nullptr, data_.size());
}

// TODO(manzagop): Collection should factor in whether this is a true crash or
// dump without crashing.
bool CollectStabilityReport(
    std::unique_ptr<base::debug::GlobalActivityAnalyzer> global_analyzer,
    StabilityReport* report) {
  CollectionStatus status = ANALYZER_CREATION_FAILED;
  if (global_analyzer)
    status = Extract(std::move(global_analyzer), report);

  base::UmaHistogramEnumeration("ActivityTracker.CollectCrash.Status", status,
                                COLLECTION_STATUS_MAX);
  if (status != SUCCESS)
    return false;

  LogCollectOnCrashEvent(CollectOnCrashEvent::kReportExtractionSuccess);

  return true;
}

void CollectSystemPerformanceMetrics(StabilityReport* report) {
  // Grab system commit memory. Also best effort.
  PERFORMANCE_INFORMATION perf_info = {sizeof(perf_info)};
  if (GetPerformanceInfo(&perf_info, sizeof(perf_info))) {
    auto* memory_state =
        report->mutable_system_memory_state()->mutable_windows_memory();

    memory_state->set_system_commit_limit(perf_info.CommitLimit);
    memory_state->set_system_commit_remaining(perf_info.CommitLimit -
                                              perf_info.CommitTotal);
    memory_state->set_system_handle_count(perf_info.HandleCount);
  }
}

void CollectProcessPerformanceMetrics(
    crashpad::ProcessSnapshot* process_snapshot,
    StabilityReport* report) {
  const crashpad::ExceptionSnapshot* exception = process_snapshot->Exception();

  if (!exception)
    return;

  // Find or create the ProcessState for the process in question.
  base::ProcessId pid = process_snapshot->ProcessID();
  ProcessState* process_state = nullptr;
  for (int i = 0; i < report->process_states_size(); ++i) {
    ProcessState* temp = report->mutable_process_states(i);

    if (temp->has_process_id() && temp->process_id() == pid) {
      process_state = temp;
      break;
    }
  }

  if (!process_state) {
    process_state = report->add_process_states();
    process_state->set_process_id(pid);
  }

  auto* memory_state =
      process_state->mutable_memory_state()->mutable_windows_memory();

  // Grab the requested allocation size in case of OOM exception.
  if (exception->Exception() == base::win::kOomExceptionCode) {
    const auto& codes = exception->Codes();
    if (codes.size()) {
      // The first parameter, if present, is the size of the allocation attempt.
      memory_state->set_process_allocation_attempt(codes[0]);
    }
  }

  base::Process process(base::Process::OpenWithAccess(
      pid, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ));

  if (process.IsValid()) {
    PROCESS_MEMORY_COUNTERS_EX process_memory = {sizeof(process_memory)};
    if (::GetProcessMemoryInfo(
            process.Handle(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&process_memory),
            sizeof(process_memory))) {
      // This is in units of bytes, re-scale to pages for consistency with
      // system metrics.
      const uint64_t kPageSize = 4096;
      memory_state->set_process_private_usage(process_memory.PrivateUsage /
                                              kPageSize);
      memory_state->set_process_peak_workingset_size(
          process_memory.PeakWorkingSetSize / kPageSize);
      memory_state->set_process_peak_pagefile_usage(
          process_memory.PeakPagefileUsage / kPageSize);
    }

    DWORD process_handle_count = 0;
    if (::GetProcessHandleCount(process.Handle(), &process_handle_count)) {
      memory_state->set_process_handle_count(process_handle_count);
    }
  }
}

// If the process has a beacon for in-memory activities, returns an analyzer
// for it.
std::unique_ptr<base::debug::GlobalActivityAnalyzer>
MaybeGetInMemoryActivityAnalyzer(crashpad::ProcessSnapshot* process_snapshot) {
  if (!process_snapshot->Memory())
    return nullptr;

  auto modules = process_snapshot->Modules();
  for (auto* module : modules) {
    auto annotations = module->AnnotationObjects();
    for (const auto& annotation : annotations) {
      if (annotation.name == ActivityTrackerAnnotation::kAnnotationName &&
          annotation.type == static_cast<uint16_t>(
                                 ActivityTrackerAnnotation::kAnnotationType) &&
          annotation.value.size() ==
              sizeof(ActivityTrackerAnnotation::ValueType)) {
        // Re-cast the annotation to its value type.
        ActivityTrackerAnnotation::ValueType value;
        memcpy(&value, annotation.value.data(), sizeof(value));

        // Check the size field for sanity.
        if (value.size > kMaxActivityAnnotationSize)
          continue;

        // Allocate the buffer with no terminate-on-exhaustion to make sure
        // this can't be used to bring down the handler and thus elide
        // crash reporting.
        UniqueMallocPtr buffer = UncheckedAllocate(value.size);
        if (!buffer || !base::PersistentMemoryAllocator::IsMemoryAcceptable(
                           buffer.get(), value.size, 0, true)) {
          continue;
        }

        // Read the activity tracker data from the crashed process.
        if (process_snapshot->Memory()->Read(value.address, value.size,
                                             buffer.get())) {
          // Success - wrap an allocator on the buffer, and an analyzer on that.
          std::unique_ptr<MallocMemoryAllocator> allocator =
              std::make_unique<MallocMemoryAllocator>(std::move(buffer),
                                                      value.size);

          return base::debug::GlobalActivityAnalyzer::CreateWithAllocator(
              std::move(allocator));
        } else {
          return nullptr;
        }
      }
    }
  }

  return nullptr;
}

}  // namespace

ActivityReportUserStreamDataSource::ActivityReportUserStreamDataSource(
    const base::FilePath& user_data_dir)
    : user_data_dir_(user_data_dir) {}

std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
ActivityReportUserStreamDataSource::ProduceStreamData(
    crashpad::ProcessSnapshot* process_snapshot) {
  DCHECK(process_snapshot);
  LogCollectOnCrashEvent(CollectOnCrashEvent::kCollectAttempt);

  StabilityReport report;

  // See whether there's an activity tracking report beacon in the process'
  // annotations.
  std::unique_ptr<base::debug::GlobalActivityAnalyzer> global_analyzer =
      MaybeGetInMemoryActivityAnalyzer(process_snapshot);
  bool collected_report = false;
  if (global_analyzer) {
    LogCollectOnCrashEvent(CollectOnCrashEvent::kInMemoryAnnotationExists);

    collected_report =
        CollectStabilityReport(std::move(global_analyzer), &report);
  }

  CollectSystemPerformanceMetrics(&report);
  CollectProcessPerformanceMetrics(process_snapshot, &report);

  std::unique_ptr<BufferExtensionStreamDataSource> source(
      new BufferExtensionStreamDataSource(kActivityReportStreamType));
  if (!source->Init(report))
    return nullptr;

  if (collected_report)
    LogCollectOnCrashEvent(CollectOnCrashEvent::kSuccess);

  return source;
}

}  // namespace browser_watcher
