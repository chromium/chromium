// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_SCOPED_TEMP_DIR_WITH_RETRY_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_SCOPED_TEMP_DIR_WITH_RETRY_H_

#include "base/files/scoped_temp_dir.h"

// An object representing a temporary / scratch directory that should be cleaned
// up (recursively) when this object goes out of scope.  Automatically retries
// if directory cleanup fails.
class ScopedTempDirWithRetry : public base::ScopedTempDir {
 public:
  ScopedTempDirWithRetry();
  ScopedTempDirWithRetry(const ScopedTempDirWithRetry&) = delete;
  ScopedTempDirWithRetry& operator=(const ScopedTempDirWithRetry&) = delete;
  ~ScopedTempDirWithRetry();
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_SCOPED_TEMP_DIR_WITH_RETRY_H_
