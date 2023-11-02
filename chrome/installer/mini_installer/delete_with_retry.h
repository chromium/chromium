// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER_DELETE_WITH_RETRY_H_
#define CHROME_INSTALLER_MINI_INSTALLER_DELETE_WITH_RETRY_H_

namespace mini_installer {

// Deletes the file or directory at |path|, retrying for up to ten seconds.
// Returns false if |path| indicates a directory that is not empty or if the
// item could not be deleted after ten seconds of effort. Otherwise, returns
// true once the item has been deleted. |attempts| is populated with the number
// of attempts needed to reach the result (e.g., a value of 1 means that the
// item was deleted on the first attempt and no sleeping was required).
bool DeleteWithRetry(const wchar_t* path, int& attempts);

// Sets a function with accompanying context that will be called by
// DeleteWithRetry when it sleeps before a retry. Returns the previous sleep
// function.
using SleepFunction = void (*)(void*);
SleepFunction SetRetrySleepHookForTesting(SleepFunction hook_fn,
                                          void* hook_context);

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER_DELETE_WITH_RETRY_H_
