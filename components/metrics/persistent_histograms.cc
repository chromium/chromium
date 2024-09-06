// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/persistent_histograms.h"

#include <string_view>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/metrics/persistent_system_profile.h"

namespace {
// Creating a "spare" file for persistent metrics involves a lot of I/O and
// isn't important so delay the operation for a while after startup.
#if BUILDFLAG(IS_ANDROID)
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

#if BUILDFLAG(IS_WIN)

// Windows sometimes creates files of the form MyFile.pma~RF71cb1793.TMP
// when trying to rename a file to something that exists but is in-use, and
// then fails to remove them. See https://crbug.com/934164
void DeleteOldWindowsTempFiles(const base::FilePath& dir) {
  // Look for any temp files older than one day and remove them. The time check
  // ensures that nothing in active transition gets deleted; these names only
  // exists on the order of milliseconds when working properly so "one day" is
  // generous but still ensures no big build up of these files. This is an
  // I/O intensive task so do it in the background (enforced by "file" calls).
  base::Time one_day_ago = base::Time::Now() - base::Days(1);
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
constexpr base::TimeDelta kDeleteOldWindowsTempFilesDelay = base::Minutes(2);

#endif  // BUILDFLAG(IS_WIN)

// Create persistent/shared memory and allow histograms to be stored in
// it. Memory that is not actually used won't be physically mapped by the
// system. BrowserMetrics usage, as reported in UMA, has the 99.99
// percentile around 3MiB as of 2018-10-22.
// Please update ServicificationBackgroundServiceTest.java if the |kAllocSize|
// is changed.
// LINT.IfChange
const size_t kAllocSize = 4 << 20;     // 4 MiB
const uint32_t kAllocId = 0x935DDD43;  // SHA1(BrowserMetrics)

base::FilePath GetSpareFilePath(const base::FilePath& metrics_dir) {
  return base::GlobalHistogramAllocator::ConstructFilePath(
      metrics_dir, kBrowserMetricsName + std::string("-spare"));
}
// LINT.ThenChange(/chrome/android/java/src/org/chromium/chrome/browser/backup/ChromeBackupAgentImpl.java)

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
  base::FilePath spare_file = GetSpareFilePath(metrics_dir);
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
    // Disallow multiple writers (Windows only). Needed to ensure multiple
    // instances of Chrome aren't writing to the same file, which could happen
    // in some rare circumstances observed in the wild (e.g. on FAT FS where the
    // file name ends up not being unique due to truncation and two processes
    // racing on base::PathExists(active_file) above).
    const bool exclusive_write = true;
    // Move any spare file into the active position.
    base::ReplaceFile(spare_file, active_file, nullptr);
    // Create global allocator using the |active_file|.
    if (kSpareFileRequired && !base::PathExists(active_file)) {
      result = kNoSpareFile;
    } else if (base::GlobalHistogramAllocator::CreateWithFile(
                   active_file, kAllocSize, kAllocId, kBrowserMetricsName,
                   exclusive_write)) {
      result = kMappedFileSuccess;
    } else {
      result = kMappedFileFailed;
    }
  }

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
  // TODO(crbug.com/40751882): Only create the dir in kMappedFile mode.
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
      // TODO(crbug.com/40751882): Log the histogram below in this case too.
      return;
  }

  // Get the allocator that was just created and report result. Exit if the
  // allocator could not be created.
  base::UmaHistogramEnumeration("UMA.PersistentHistograms.InitResult", result);

  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  if (!allocator) {
    // If no allocator was created above, try to create a LocalMemory one here.
    // This avoids repeating the call many times above. In the case where
    // persistence is disabled, an early return is done above.
    base::GlobalHistogramAllocator::CreateWithLocalMemory(kAllocSize, kAllocId,
                                                          kBrowserMetricsName);
    allocator = base::GlobalHistogramAllocator::Get();
    if (!allocator) {
      return;
    }
  }

  // Store a copy of the system profile in this allocator.
  metrics::GlobalPersistentSystemProfile::GetInstance()
      ->RegisterPersistentAllocator(allocator->memory_allocator());

  // Create tracking histograms for the allocator and record storage file.
  allocator->CreateTrackingHistograms(kBrowserMetricsName);
}

}  // namespace

BASE_FEATURE(
    kPersistentHistogramsFeature,
    "PersistentHistograms",
#if BUILDFLAG(IS_FUCHSIA)
    // TODO(crbug.com/42050425): Enable once writable mmap() is supported. Also
    // move the initialization earlier to chrome/app/chrome_main_delegate.cc.
    base::FEATURE_DISABLED_BY_DEFAULT
#else
    base::FEATURE_ENABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_FUCHSIA)
);

const char kPersistentHistogramStorageMappedFile[] = "MappedFile";
const char kPersistentHistogramStorageLocalMemory[] = "LocalMemory";

const base::FeatureParam<std::string> kPersistentHistogramsStorage{
    &kPersistentHistogramsFeature, "storage",
    kPersistentHistogramStorageMappedFile};

const char kBrowserMetricsName[] = "BrowserMetrics";
const char kDeferredBrowserMetricsName[] = "DeferredBrowserMetrics";

void InstantiatePersistentHistograms(const base::FilePath& metrics_dir,
                                     bool persistent_histograms_enabled,
                                     std::string_view storage) {
  PersistentHistogramsMode mode = kNotEnabled;
  // Note: The extra feature check is needed so that we don't use the default
  // value of the storage param if the feature is disabled.
  if (persistent_histograms_enabled) {
    if (storage == kPersistentHistogramStorageMappedFile) {
      mode = kMappedFile;
    } else if (storage == kPersistentHistogramStorageLocalMemory) {
      mode = kLocalMemory;
    }
  }

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
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

void PersistentHistogramsCleanup(const base::FilePath& metrics_dir) {
  base::FilePath spare_file = GetSpareFilePath(metrics_dir);

  // Schedule the creation of a "spare" file for use on the next run.
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::LOWEST,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          base::IgnoreResult(&base::GlobalHistogramAllocator::CreateSpareFile),
          std::move(spare_file), kAllocSize),
      base::Seconds(kSpareFileCreateDelaySeconds));

#if BUILDFLAG(IS_WIN)
  // Post a best effort task that will delete files. Unlike SKIP_ON_SHUTDOWN,
  // which will block on the deletion if the task already started,
  // CONTINUE_ON_SHUTDOWN will not block shutdown on the task completing. It's
  // not a *necessity* to delete the files the same session they are "detected".
  // On shutdown, the deletion will be interrupted.
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DeleteOldWindowsTempFiles, metrics_dir),
      kDeleteOldWindowsTempFilesDelay);
#endif  // BUILDFLAG(IS_WIN)
}

void InstantiatePersistentHistogramsWithFeaturesAndCleanup(
    const base::FilePath& metrics_dir) {
  InstantiatePersistentHistograms(
      metrics_dir, base::FeatureList::IsEnabled(kPersistentHistogramsFeature),
      kPersistentHistogramsStorage.Get());
  PersistentHistogramsCleanup(metrics_dir);
}

bool DeferBrowserMetrics(const base::FilePath& metrics_dir) {
  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();

  if (!allocator || !allocator->HasPersistentLocation()) {
    return false;
  }

  base::FilePath deferred_metrics_dir =
      metrics_dir.AppendASCII(kDeferredBrowserMetricsName);

  if (!base::CreateDirectory(deferred_metrics_dir)) {
    return false;
  }

  return allocator->MovePersistentFile(deferred_metrics_dir);
}
