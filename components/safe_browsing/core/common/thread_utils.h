// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_THREAD_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_THREAD_UTILS_H_

#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/task/task_traits.h"

namespace safe_browsing {

// Safe Browsing thread-related utility functions, which need separate
// implementations for content/ clients and for ios/ since content/ and ios/
// use distinct classes for representing threads (content::BrowserThread and
// web::WebThread).

// An enumeration of well-known threads.
enum class ThreadID {
  // The main thread in the browser.
  UI,

  // The thread that processes non-blocking IO (IPC and network).
  IO,
};

// Callable on any thread. Returns true if the current thread matches the given
// identifier.
bool CurrentlyOnThread(ThreadID thread_id) WARN_UNUSED_RESULT;

// Callable on any thread. Returns the task runner associated with the given
// identifier.
scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(ThreadID thread_id);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_THREAD_UTILS_H_
