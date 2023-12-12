// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/scoped_temp_dir_with_retry.h"

#include "base/logging.h"
#include "base/threading/platform_thread.h"

ScopedTempDirWithRetry::ScopedTempDirWithRetry() = default;

ScopedTempDirWithRetry::~ScopedTempDirWithRetry() {
  if (!IsValid()) {
    return;
  }

  int retry = 0;
  while (!Delete()) {
    // Delete failed. Retry up to 100 times, with 10 ms delay between each
    // retry (thus maximum delay is about 1 second).
    if (++retry > 100) {
      DLOG(WARNING) << "Could not delete temp dir after retries.";
      break;
    }
    base::PlatformThread::Sleep(base::Milliseconds(10));
  }
}
