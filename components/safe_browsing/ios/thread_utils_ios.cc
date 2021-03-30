// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/thread_utils.h"

#include "base/task/post_task.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

using web::WebThread;

namespace safe_browsing {

static WebThread::ID WebThreadID(ThreadID thread_id) {
  switch (thread_id) {
    case ThreadID::UI:
      return WebThread::UI;
    case ThreadID::IO:
      return WebThread::IO;
  }
}

bool CurrentlyOnThread(ThreadID thread_id) {
  return WebThread::CurrentlyOn(WebThreadID(thread_id));
}

scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(ThreadID thread_id) {
  return base::CreateSingleThreadTaskRunner({WebThreadID(thread_id)});
}

}  // namespace safe_browsing
