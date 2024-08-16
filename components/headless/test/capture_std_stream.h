// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEADLESS_TEST_CAPTURE_STD_STREAM_H_
#define COMPONENTS_HEADLESS_TEST_CAPTURE_STD_STREAM_H_

#include <cstdio>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_restrictions.h"

namespace headless {

// A class to capture data sent to a standard stream.
class CaptureStdStream {
 public:
  explicit CaptureStdStream(FILE* stream);
  ~CaptureStdStream();

  void StartCapture();
  void StopCapture();

  std::string TakeCapturedData();

 private:
  raw_ptr<FILE> stream_;

  int fileno_ = -1;
  std::array<int, 2> pipes_ = {-1, -1};
  bool capturing_ = false;

  // TODO(https://github.com/llvm/llvm-project/issues/61334): Explicit
  // [[maybe_unused]] attribute shouuld not be necessary here.
  [[maybe_unused]] base::ScopedAllowBlockingForTesting allow_blocking_calls_;
};

class CaptureStdOut : public CaptureStdStream {
 public:
  CaptureStdOut() : CaptureStdStream(stdout) {}
};

class CaptureStdErr : public CaptureStdStream {
 public:
  CaptureStdErr() : CaptureStdStream(stderr) {}
};

}  // namespace headless

#endif  // COMPONENTS_HEADLESS_TEST_CAPTURE_STD_STREAM_H_
