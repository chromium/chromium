// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/delete_with_retry.h"

#include <windows.h>

#include <utility>

namespace mini_installer {

namespace {

// A hook function (and accompanying context) for testing that is called when
// it's time to sleep before a retry.
SleepFunction g_hook_fn = nullptr;
void* g_hook_context = nullptr;

// Returns true if |error| is conceivably transient, and that it's reasonable to
// believe that retrying the operation could result in a different error.
bool IsTransientFailure(DWORD error) {
  return
      // ACCESS_DENIED could mean that the item has its read-only attribute set,
      // that it is mapped into a process's address space, or that the item has
      // been deleted but open handles remain.
      error == ERROR_ACCESS_DENIED ||
      // SHARING_VIOLATION generally means that there exists an open handle
      // without SHARE_DELETE.
      error == ERROR_SHARING_VIOLATION ||
      // A directory is not considered empty until deletes of all items within
      // it have been finalized.
      error == ERROR_DIR_NOT_EMPTY;
}

// Marks the file or directory at |path| for deletion. Returns true if the file
// was present and was successfully marked, or false (populating |error| with a
// Windows error code) otherwise.
bool MarkForDeletion(const wchar_t* path, DWORD& error) {
  // While it's tempting to use FILE_FLAG_DELETE_ON_CLOSE, doing so hides the
  // success or failure of marking the file. Opening the file could fail if
  // another process has it open without FILE_SHARE_DELETE.
  HANDLE handle = ::CreateFileW(
      path, DELETE, FILE_SHARE_DELETE | FILE_SHARE_WRITE | FILE_SHARE_READ,
      /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
      /*hTemplateFile=*/nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    error = ::GetLastError();
    return false;  // Failed to open the file.
  }

  // Mark the file for deletion. On success, the file will be deleted when all
  // open handles are closed. This could fail if another process has the file
  // mapped into its address space, among other reasons.
  FILE_DISPOSITION_INFO disposition = {/*DeleteFile=*/TRUE};
  const bool succeeded =
      ::SetFileInformationByHandle(handle, FileDispositionInfo, &disposition,
                                   sizeof(disposition)) != 0;
  if (!succeeded)
    error = ::GetLastError();

  ::CloseHandle(handle);
  return succeeded;
}

// Clears the read-only attribute of |path|. Returns true if |path| was
// read-only and has had this attribute cleared, or false otherwise (e.g.,
// |path| names a file without the read-only attribute, |path| could not be
// opened).
bool ClearReadOnly(const wchar_t* path) {
  HANDLE handle =
      ::CreateFileW(path, FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
                    FILE_SHARE_DELETE | FILE_SHARE_WRITE | FILE_SHARE_READ,
                    /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                    /*hTemplateFile=*/nullptr);
  if (handle == INVALID_HANDLE_VALUE)
    return false;  // Not modified.

  bool modified = false;
  FILE_BASIC_INFO info = {};

  // Clear the read-only attribute if |path| names a file with it set.
  if (::GetFileInformationByHandleEx(handle, FileBasicInfo, &info,
                                     sizeof(info)) != 0 &&
      (info.FileAttributes & FILE_ATTRIBUTE_READONLY) != 0) {
    info.FileAttributes &= ~FILE_ATTRIBUTE_READONLY;
    modified = ::SetFileInformationByHandle(handle, FileBasicInfo, &info,
                                            sizeof(info)) != 0;
  }

  ::CloseHandle(handle);
  return modified;
}

}  // namespace

// This function does what we wish ::DeleteFile did, but obviously can't. It
// patiently waits for other processes to finish operating on the file/dir and
// only returns when the file/dir is truly gone. Since it blocks the calling
// thread for up to ten seconds, it is only suitable for specific cases like
// here in mini_installer where blocking is the desired behavior.
bool DeleteWithRetry(const wchar_t* path, int& attempts) {
  constexpr DWORD kRetryPeriodMs = 100;  // Wait 100ms between retries.
  constexpr int kMaxAttempts = (10 * 1000) / kRetryPeriodMs;  // Retry for 10s.

  attempts = 1;
  while (true) {
    DWORD error;
    if (MarkForDeletion(path, error)) {
      // The item has been marked for deletion on close. It will not be deleted
      // until all open handles across all processes have been closed. In the
      // meantime, the name cannot be reused (attempts to create a new file with
      // the same name will fail) and the containing directory cannot be deleted
      // (it is not empty, after all). Before returning success to the caller,
      // at least one more attempt to access the item must be made to determine
      // whether or not deletion has taken place. This attempt is likely to
      // return ACCESS_DENIED if handles remain open, FILE_NOT_FOUND if all
      // handles have been closed, or even succss or SHARING_VIOLATION if the
      // delete-on-close bit has been cleared through another open handle. To
      // handle all of these cases, go back to the start of this loop to retry
      // deletion immediately.
      continue;
    }
    // The attempt to mark the file for deletion on close has failed.

    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
      return true;  // Because the item doesn't exist -- the success case.

    if (!IsTransientFailure(error))
      return false;  // There is no point in trying again.

    // Try to clear the read-only attribute if there is probable cause.
    if (error == ERROR_ACCESS_DENIED && ClearReadOnly(path))
      continue;  // Make another attempt to delete the file without delay.

    if (attempts == kMaxAttempts)
      break;  // Enough is enough.

    // Try again after letting other processes finish with the file/dir.
    ++attempts;
    if (g_hook_fn)
      (*g_hook_fn)(g_hook_context);
    else
      ::Sleep(kRetryPeriodMs);
  }

  return false;  // Failed all retries
}

SleepFunction SetRetrySleepHookForTesting(SleepFunction hook_fn,
                                          void* hook_context) {
  g_hook_context = hook_context;
  return std::exchange(g_hook_fn, hook_fn);
}

}  // namespace mini_installer
