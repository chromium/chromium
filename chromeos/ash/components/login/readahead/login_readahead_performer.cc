// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/readahead/login_readahead_performer.h"

#include <fcntl.h>

#include <algorithm>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"

namespace ash {
namespace {
// Critical files for login and post login. This list should be updated
// whenever one of them is renamed or removed.
constexpr const char* kFilePaths[] = {
    "/opt/google/chrome/chrome",
    "/opt/google/chrome/resources.pak",
    "/opt/google/chrome/chrome_100_percent.pak",
    "/opt/google/chrome/chrome_200_percent.pak",
    "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/roboto/Roboto-Medium.ttf",
    "/usr/share/fonts/roboto/Roboto-Regular.ttf"};

constexpr const char* kUmaResultName = "Ash.LoginReadahead.Result";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LoginReadaheadResult)
enum class LoginReadaheadResult {
  kFailed = 0,
  kSucceeded = 1,
  kCanceled = 2,
  kMaxValue = kCanceled,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ash/enums.xml:LoginReadaheadResult)

// TODO(crbug.com/378608072): Consider using `base::PreReadFile`. However, the
// implementation for Linux may have an issue where only some beginning part can
// be prefetched. We should investigate and hopefully replace this function with
// it.
LoginReadaheadResult ReadaheadEntireFile(
    const base::FilePath& file_path,
    scoped_refptr<CancelNotifier> notifier) {
  if (notifier && notifier->IsCancelRequested()) {
    return LoginReadaheadResult::kCanceled;
  }

  base::File file(file_path,
                  base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  if (!file.IsValid()) {
    PLOG(ERROR) << "Failed to open file. Possibly the file has been removed or "
                   "renamed: path="
                << file_path;
    return LoginReadaheadResult::kFailed;
  }

  base::File::Info file_info;
  if (!file.GetInfo(&file_info)) {
    PLOG(ERROR)
        << "GetInfo should not fail once file is opened successfully: path="
        << file_path;
    return LoginReadaheadResult::kFailed;
  }

  const base::PlatformFile fd = file.GetPlatformFile();

  // Linux kernel allows readahead/posix_fadvise with a large number of bytes,
  // but it is not guaranteed that the requested ranges will be loaded into
  // cache since the size is considered as a hint. Thus, we call readahead
  // multiple times with a sufficiently small window size here to readahead as
  // much as possible.

  // The maximum window size seems at least 128KiB and `ureadahead` uses the
  // 128KiB window size too.
  constexpr size_t kMaxWindowSize = 128 * 1024;

  size_t remaining = base::checked_cast<size_t>(file_info.size);
  off_t offset = 0;

  while (remaining > 0) {
    if (notifier && notifier->IsCancelRequested()) {
      return LoginReadaheadResult::kCanceled;
    }

    size_t window_size = std::min(remaining, kMaxWindowSize);

    // NOTE: readahead returns quickly without waiting for the requested range
    // to be loaded from disk.
    if (readahead(fd, offset, window_size) == -1) {
      PLOG(ERROR) << "readahead failed: path=" << file_path
                  << " (size=" << file_info.size << "), offset=" << offset
                  << ", window_size=" << window_size;
      return LoginReadaheadResult::kFailed;
    }
    offset += window_size;
    remaining -= window_size;
  }

  return LoginReadaheadResult::kSucceeded;
}

// Perform readahead based on the predefined list.
LoginReadaheadResult ReadaheadFiles(scoped_refptr<CancelNotifier> notifier) {
  LoginReadaheadResult result = LoginReadaheadResult::kSucceeded;

  for (const char* path : kFilePaths) {
    switch (ReadaheadEntireFile(base::FilePath(path), notifier)) {
      case LoginReadaheadResult::kCanceled:
        return LoginReadaheadResult::kCanceled;

      case LoginReadaheadResult::kFailed:
        // Continue to next file while marking the final result as failure.
        result = LoginReadaheadResult::kFailed;
        continue;

      case LoginReadaheadResult::kSucceeded:
        break;
    }
  }

  return result;
}

void ReadaheadFilesAndReportResult(scoped_refptr<CancelNotifier> notifier) {
  LoginReadaheadResult result = ReadaheadFiles(std::move(notifier));
  base::UmaHistogramEnumeration(kUmaResultName, result);
}

}  // namespace

LoginReadaheadPerformer::LoginReadaheadPerformer(
    SessionManagerClient* session_manager_client) {
  CHECK(session_manager_client);
  scoped_observation_.Observe(session_manager_client);
}

LoginReadaheadPerformer::~LoginReadaheadPerformer() = default;

void LoginReadaheadPerformer::EmitLoginPromptVisibleCalled() {
  // Start readahead task.
  cancel_notifier_ = base::MakeRefCounted<CancelNotifier>();
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(ReadaheadFilesAndReportResult, cancel_notifier_));
}

void LoginReadaheadPerformer::StartSessionExCalled() {
  // Cancel the readahead task to avoid conflicts with critical tasks for login.
  // Note that this cannot cancel readahead work that has been already requested
  // via the readahead system call because it is async operation and there is no
  // way to cancel the kernel tasks once readahead returns.
  if (cancel_notifier_) {
    cancel_notifier_->Cancel();
    cancel_notifier_.reset();
  }
}

}  // namespace ash
