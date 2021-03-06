// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/persistent_histograms.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/variations/variations_associated_data.h"

namespace {

// Creating a "spare" file for persistent metrics involves a lot of I/O and
// isn't important so delay the operation for a while after startup.
#if defined(OS_ANDROID)
// Android needs the spare file and also launches faster.
constexpr bool kSpareFileRequired = true;
constexpr int kSpareFileCreateDelaySeconds = 10;
#else
// Desktop may have to restore a lot of tabs so give it more time before doing
// non-essential work. The spare file is still a performance boost but not as
// significant of one so it's not required.
constexpr bool kSpareFileRequired = false;
constexpr int kSpareFileCreateDelaySeconds = 90;
#endif

#if defined(OS_WIN)

// Windows sometimes creates files of the form MyFile.pma~RF71cb1793.TMP
// when trying to rename a file to something that exists but is in-use, and
// then fails to remove them. See https://crbug.com/934164
void DeleteOldWindowsTempFiles(const base::FilePath& dir) {
  // Look for any temp files older than one day and remove them. The time check
  // ensures that nothing in active transition gets deleted; these names only
  // exists on the order of milliseconds when working properly so "one day" is
  // generous but still ensures no big build up of these files. This is an
  // I/O intensive task so do it in the background (enforced by "file" calls).
  base::Time one_day_ago = base::Time::Now() - base::TimeDelta::FromDays(1);
  base::FileEnumerator file_iter(dir, /*recursive=*/false,
                                 base::FileEnumerator::FILES);
  for (base::FilePath path = file_iter.Next(); !path.empty();
       path = file_iter.Next()) {
    if (base::ToUpperASCII(path.FinalExtension()) !=
            FILE_PATH_LITERAL(".TMP") ||
        base::ToUpperASCII(path.BaseName().value())
                .find(FILE_PATH_LITERAL(".PMA~RF")) < 0) {
      continue;
    }

    const auto& info = file_iter.GetInfo();
    if (info.IsDirectory())
      continue;
    if (info.GetLastModifiedTime() > one_day_ago)
      continue;

    base::DeleteFile(path);
  }
}

// How much time after startup to run the above function. Two minutes is
// enough for the system to stabilize and get the user what they want before
// spending time on clean-up efforts.
constexpr base::TimeDelta kDeleteOldWindowsTempFilesDelay =
    base::TimeDelta::FromMinutes(2);

#endif  // defined(OS_WIN)

// Create persistent/shared memory and allow histograms to be stored in
// it. Memory that is not actually used won't be physically mapped by the
// system. BrowserMetrics usage, as reported in UMA, has the 99.99
// percentile around 3MiB as of 2018-10-22.
// Please update ServicificationBackgroundServiceTest.java if the |kAllocSize|
// is changed.
const size_t kAllocSize = 4 << 20;     // 4 MiB
const uint32_t kAllocId = 0x935DDD43;  // SHA1(BrowserMetrics)

// Logged to UMA - keep in sync with enums.xml.
enum InitResult {
  kLocalMemorySuccess,
  kLocalMemoryFailed,
  kMappedFileSuccess,
  kMappedFileFailed,
  kMappedFileExists,
  kNoSpareFile,
  kNoUploadDir,
  kMaxValue = kNoUploadDir
};

// Initializes persistent histograms with a memory-mapped file.
InitResult InitWithMappedFile(const base::FilePath& metrics_dir,
                              const base::FilePath& upload_dir) {
  // The spare file in the user data dir ("BrowserMetrics-spare.pma") would
  // have been created in the previous session. We will move it to |upload_dir|
  // and rename it with the current time and process id for use as |active_file|
  // (e.g. "BrowserMetrics/BrowserMetrics-1234ABCD-12345.pma").
  // Any unreported metrics in this file will be uploaded next session.
  base::FilePath spare_file = base::GlobalHistogramAllocator::ConstructFilePath(
      metrics_dir, kBrowserMetricsName + std::string("-spare"));
  base::FilePath active_file =
      base::GlobalHistogramAllocator::ConstructFilePathForUploadDir(
          upload_dir, kBrowserMetricsName, base::Time::Now(),
          base::GetCurrentProcId());

  InitResult result;
  if (!base::PathExists(upload_dir)) {
    // Handle failure to create the directory.
    result = kNoUploadDir;
  } else if (base::PathExists(active_file)) {
    // "active" filename is supposed to be unique so this shouldn't happen.
    result = kMappedFileExists;
  } else {
    // Move any spare file into the active position.
    // TODO(crbug.com/1183166): Don't do this if |kSpareFileRequired| = false.
    base::ReplaceFile(spare_file, active_file, nullptr);
    // Create global allocator using the |active_file|.
    if (kSpareFileRequired && !base::PathExists(active_file)) {
      result = kNoSpareFile;
    } else if (base::GlobalHistogramAllocator::CreateWithFile(
                   active_file, kAllocSize, kAllocId, kBrowserMetricsName)) {
      // TODO(crbug.com/1176977): Remove this instrumentation when bug is fixed.
      // We don't expect there to be any histograms in the file just opened. But
      // if there are, log their hashes here to diagnose crbug.com/1176977.
      base::PersistentHistogramAllocator::Iterator it(
          base::GlobalHistogramAllocator::Get());
      while (std::unique_ptr<base::HistogramBase> histogram = it.GetNext()) {
        base::UmaHistogramSparse(
            "UMA.PersistentHistograms.HistogramsInStartupFile",
            static_cast<base::HistogramBase::Sample>(histogram->name_hash()));
      }
      result = kMappedFileSuccess;
    } else {
      result = kMappedFileFailed;
    }
  }
  // Schedule the creation of a "spare" file for use on the next run.
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::LOWEST,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          base::IgnoreResult(&base::GlobalHistogramAllocator::CreateSpareFile),
          std::move(spare_file), kAllocSize),
      base::TimeDelta::FromSeconds(kSpareFileCreateDelaySeconds));

  return result;
}

enum PersistentHistogramsMode {
  kNotEnabled,
  kMappedFile,
  kLocalMemory,
};

// Implementation of InstantiatePersistentHistograms() that does the work after
// the desired |mode| has been determined.
void InstantiatePersistentHistogramsImpl(const base::FilePath& metrics_dir,
                                         PersistentHistogramsMode mode) {
  // Create a directory for storing completed metrics files. Files in this
  // directory must have embedded system profiles. If the directory can't be
  // created, the file will just be deleted below.
  base::FilePath upload_dir = metrics_dir.AppendASCII(kBrowserMetricsName);
  // TODO(crbug.com/1183166): Only create the dir in kMappedFile mode.
  base::CreateDirectory(upload_dir);

  InitResult result;

  // Create a global histogram allocator using the desired storage type.
  switch (mode) {
    case kMappedFile:
      result = InitWithMappedFile(metrics_dir, upload_dir);
      break;
    case kLocalMemory:
      // Use local memory for storage even though it will not persist across
      // an unclean shutdown. This sets the result but the actual creation is
      // done below.
      result = kLocalMemorySuccess;
      break;
    case kNotEnabled:
      // Persistent metric storage is disabled. Must return here.
      // TODO(crbug.com/1183166): Run DeleteOldWindowsTempFiles() here too.
      // TODO(crbug.com/1183166): Log the histogram below in this case too.
      return;
  }

  // Get the allocator that was just created and report result. Exit if the
  // allocator could not be created.
  base::UmaHistogramEnumeration("UMA.PersistentHistograms.InitResult", result);

  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  if (!allocator) {
    // If no allocator was created above, try to create a LocalMemomory one
    // here. This avoids repeating the call many times above. In the case where
    // persistence is disabled, an early return is done above.
    base::GlobalHistogramAllocator::CreateWithLocalMemory(kAllocSize, kAllocId,
                                                          kBrowserMetricsName);
    allocator = base::GlobalHistogramAllocator::Get();
    if (!allocator) {
      // TODO(crbug.com/1183166): Run DeleteOldWindowsTempFiles() here too.
      return;
    }
  }

  // Store a copy of the system profile in this allocator.
  metrics::GlobalPersistentSystemProfile::GetInstance()
      ->RegisterPersistentAllocator(allocator->memory_allocator());

  // Create tracking histograms for the allocator and record storage file.
  allocator->CreateTrackingHistograms(kBrowserMetricsName);

#if defined(OS_WIN)
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::LOWEST,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&DeleteOldWindowsTempFiles, std::move(metrics_dir)),
      kDeleteOldWindowsTempFilesDelay);
#endif  // defined(OS_WIN)
}

}  // namespace

const char kBrowserMetricsName[] = "BrowserMetrics";

void InstantiatePersistentHistograms(const base::FilePath& metrics_dir,
                                     bool default_local_memory) {
  // TODO(crbug.com/1183166): Enable feature by default and use its state to
  // determine if persistent histograms should be disabled. Move it out of base.
  std::string storage = variations::GetVariationParamValueByFeature(
      base::kPersistentHistogramsFeature, "storage");

  static const char kMappedFileStr[] = "MappedFile";
  static const char kLocalMemoryStr[] = "LocalMemory";

  PersistentHistogramsMode mode;
  if (storage == kMappedFileStr) {
    mode = kMappedFile;
  } else if (storage == kLocalMemoryStr) {
    mode = kLocalMemory;
  } else if (storage.empty()) {
    mode = (default_local_memory ? kLocalMemory : kMappedFile);
  } else {
    mode = kNotEnabled;
  }

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Linux kernel 4.4.0.* shows a huge number of SIGBUS crashes with persistent
  // histograms enabled using a mapped file.  Change this to use local memory.
  // https://bugs.chromium.org/p/chromium/issues/detail?id=753741
  if (mode == kMappedFile) {
    int major, minor, bugfix;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
    if (major == 4 && minor == 4 && bugfix == 0)
      mode = kLocalMemory;
  }
#endif

  InstantiatePersistentHistogramsImpl(metrics_dir, mode);
}
