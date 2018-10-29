// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/stability_paths.h"

#if defined(OS_WIN)
#include <windows.h>
#endif  // defined(OS_WIN)

#include <memory>
#include <string>
#include <utility>

#include "base/debug/activity_tracker.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/memory_mapped_file.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/browser_watcher/features.h"
#include "components/browser_watcher/stability_metrics.h"

#if defined(OS_WIN)

#include "third_party/crashpad/crashpad/util/misc/time.h"

namespace browser_watcher {

using base::FilePath;
using base::FilePersistentMemoryAllocator;
using base::MemoryMappedFile;
using base::PersistentMemoryAllocator;

namespace {

bool GetCreationTime(const base::Process& process, FILETIME* creation_time) {
  FILETIME ignore;
  return ::GetProcessTimes(process.Handle(), creation_time, &ignore, &ignore,
                           &ignore) != 0;
}

bool SetPmaFileDeleted(const base::FilePath& file_path) {
  // Map the file read-write so it can guarantee consistency between
  // the analyzer and any trackers that may still be active.
  std::unique_ptr<MemoryMappedFile> mmfile(new MemoryMappedFile());
  if (!mmfile->Initialize(file_path, MemoryMappedFile::READ_WRITE))
    return false;
  if (!FilePersistentMemoryAllocator::IsFileAcceptable(*mmfile, true))
    return false;
  FilePersistentMemoryAllocator allocator(std::move(mmfile), 0, 0,
                                          base::StringPiece(), true);
  allocator.SetMemoryState(PersistentMemoryAllocator::MEMORY_DELETED);
  return true;
}

}  // namespace

FilePath GetStabilityDir(const FilePath& user_data_dir) {
  return user_data_dir.AppendASCII("Stability");
}

FilePath GetStabilityFileForProcess(base::ProcessId pid,
                                    timeval creation_time,
                                    const FilePath& user_data_dir) {
  FilePath stability_dir = GetStabilityDir(user_data_dir);

  constexpr uint64_t kMicrosecondsPerSecond = static_cast<uint64_t>(1E6);
  int64_t creation_time_us =
      creation_time.tv_sec * kMicrosecondsPerSecond + creation_time.tv_usec;

  std::string file_name =
      base::StringPrintf("%" CrPRIdPid "-%lld", pid, creation_time_us);
  return stability_dir.AppendASCII(file_name).AddExtension(
      base::PersistentMemoryAllocator::kFileExtension);
}

bool GetStabilityFileForProcess(const base::Process& process,
                                const FilePath& user_data_dir,
                                FilePath* file_path) {
  DCHECK(file_path);

  FILETIME creation_time;
  if (!GetCreationTime(process, &creation_time))
    return false;

  // We rely on Crashpad's conversion to ensure the resulting filename is the
  // same as on crash, when the creation time is obtained via Crashpad.
  *file_path = GetStabilityFileForProcess(
      process.Pid(), crashpad::FiletimeToTimevalEpoch(creation_time),
      user_data_dir);
  return true;
}

FilePath::StringType GetStabilityFilePattern() {
  return FilePath::StringType(FILE_PATH_LITERAL("*-*")) +
         base::PersistentMemoryAllocator::kFileExtension;
}

std::vector<FilePath> GetStabilityFiles(
    const FilePath& stability_dir,
    const FilePath::StringType& stability_file_pattern,
    const std::set<FilePath>& excluded_stability_files) {
  DCHECK_NE(true, stability_dir.empty());
  DCHECK_NE(true, stability_file_pattern.empty());

  std::vector<FilePath> paths;
  base::FileEnumerator enumerator(stability_dir, false /* recursive */,
                                  base::FileEnumerator::FILES,
                                  stability_file_pattern);
  FilePath path;
  for (path = enumerator.Next(); !path.empty(); path = enumerator.Next()) {
    if (excluded_stability_files.find(path) == excluded_stability_files.end())
      paths.push_back(path);
  }
  return paths;
}

void MarkOwnStabilityFileDeleted(const base::FilePath& user_data_dir) {
  base::debug::GlobalActivityTracker* global_tracker =
      base::debug::GlobalActivityTracker::Get();
  if (!global_tracker)
    return;  // No stability instrumentation.

  global_tracker->MarkDeleted();
  LogStabilityRecordEvent(StabilityRecordEvent::kMarkDeleted);

  // Open (with delete) and then immediately close the file by going out of
  // scope. This should cause the stability debugging file to be deleted prior
  // to the next execution.
  base::FilePath stability_file;
  if (!GetStabilityFileForProcess(base::Process::Current(), user_data_dir,
                                  &stability_file)) {
    return;
  }
  LogStabilityRecordEvent(StabilityRecordEvent::kMarkDeletedGotFile);

  base::File deleter(stability_file, base::File::FLAG_OPEN |
                                         base::File::FLAG_READ |
                                         base::File::FLAG_DELETE_ON_CLOSE);
  if (!deleter.IsValid())
    LogStabilityRecordEvent(StabilityRecordEvent::kOpenForDeleteFailed);
}

void MarkStabilityFileDeletedOnCrash(const base::FilePath& file_path) {
  if (!SetPmaFileDeleted(file_path))
    LogCollectOnCrashEvent(CollectOnCrashEvent::kPmaSetDeletedFailed);

  base::File deleter(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                    base::File::FLAG_DELETE_ON_CLOSE);
  if (!deleter.IsValid())
    LogCollectOnCrashEvent(CollectOnCrashEvent::kOpenForDeleteFailed);
}

}  // namespace browser_watcher

#endif  // defined(OS_WIN)
