// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/thread_utils.h"

#include "base/notreached.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

BrowserThread::ID BrowserThreadID(ThreadID thread_id) {
  switch (thread_id) {
    case ThreadID::UI:
      return BrowserThread::UI;
    case ThreadID::IO:
      return BrowserThread::IO;
    default:
      NOTREACHED();
      return BrowserThread::UI;
  }
}

}  // namespace

bool CurrentlyOnThread(ThreadID thread_id) {
  return BrowserThread::CurrentlyOn(BrowserThreadID(thread_id));
}

base::TaskTraits CreateTaskTraits(ThreadID thread_id) {
  return {BrowserThreadID(thread_id)};
}

scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(ThreadID thread_id) {
  switch (thread_id) {
    case ThreadID::UI:
      return content::GetUIThreadTaskRunner({});
    case ThreadID::IO:
      return content::GetIOThreadTaskRunner({});
    default:
      NOTREACHED();
      return content::GetUIThreadTaskRunner({});
  }
}

}  // namespace safe_browsing
