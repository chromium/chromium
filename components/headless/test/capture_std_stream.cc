// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/test/capture_std_stream.h"

#include <fcntl.h>
#include <stdio.h>

#include "base/check_op.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace headless {

namespace {
enum { kReadPipe, kWritePipe };
static constexpr char kPipeEnd = '\xff';
}  // namespace

CaptureStdStream::CaptureStdStream(FILE* stream) : stream_(stream) {
#if BUILDFLAG(IS_WIN)
  CHECK_EQ(_pipe(pipes_.data(), 4096, O_BINARY), 0);
#else
  CHECK_EQ(pipe(pipes_.data()), 0);
#endif
  fileno_ = dup(fileno(stream_));
  CHECK_NE(fileno_, -1);
}

CaptureStdStream::~CaptureStdStream() {
  StopCapture();
  close(pipes_[kReadPipe]);
  close(pipes_[kWritePipe]);
  close(fileno_);
}

void CaptureStdStream::StartCapture() {
  if (capturing_) {
    return;
  }

  fflush(stream_);
  CHECK_NE(dup2(pipes_[kWritePipe], fileno(stream_)), -1);

  capturing_ = true;
}

void CaptureStdStream::StopCapture() {
  if (!capturing_) {
    return;
  }

  char eop = kPipeEnd;
  CHECK_NE(write(pipes_[kWritePipe], &eop, sizeof(eop)), -1);

  fflush(stream_);
  CHECK_NE(dup2(fileno_, fileno(stream_)), -1);

  capturing_ = false;
}

std::string CaptureStdStream::TakeCapturedData() {
  CHECK(!capturing_);

  std::string captured_data;
  for (;;) {
    constexpr size_t kChunkSize = 256;
    std::array<char, kChunkSize> buffer;
    int bytes_read = read(pipes_[kReadPipe], buffer.data(), kChunkSize);
    CHECK_GT(bytes_read, 0);
    std::string_view data(buffer.data(), bytes_read);
    if (data.back() != kPipeEnd) {
      captured_data.append(data);
    } else {
      data.remove_suffix(1);
      captured_data.append(data);
      break;
    }
  }

  return captured_data;
}

}  // namespace headless
