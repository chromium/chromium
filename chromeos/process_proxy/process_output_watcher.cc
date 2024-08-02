// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/process_proxy/process_output_watcher.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/single_thread_task_runner.h"
#include "base/third_party/icu/icu_utf.h"

namespace {

// Number of `unacked_outputs_` we allow before pausing.  Bigger windows are
// faster but cause jank in the renderer if we flood it.
// Tuned with `time cat big.txt` using https://norvig.com/big.txt.
constexpr int kAckWindow = 30;

// Gets byte size for a UTF8 character given it's leading byte. The character
// size is encoded as number of leading '1' bits in the character's leading
// byte. If the most significant bit is '0', the character is a valid ASCII
// and it's byte size is 1.
// The method returns 1 if the provided byte is invalid leading byte.
size_t UTF8SizeFromLeadingByte(uint8_t leading_byte) {
  size_t byte_count = 0;
  uint8_t mask = 1 << 7;
  uint8_t error_mask = 1 << (7 - CBU8_MAX_LENGTH);
  while (leading_byte & mask) {
    if (mask & error_mask)
      return 1;
    mask >>= 1;
    ++byte_count;
  }
  return byte_count ? byte_count : 1;
}

}  // namespace

namespace chromeos {

ProcessOutputWatcher::ProcessOutputWatcher(
    int out_fd,
    const ProcessOutputCallback& callback)
    : read_buffer_size_(0),
      process_output_file_(out_fd),
      on_read_callback_(callback) {
  CHECK_GE(out_fd, 0);
  // We want to be sure we will be able to add 0 at the end of the input, so -1.
  read_buffer_capacity_ = std::size(read_buffer_) - 1;
}

ProcessOutputWatcher::~ProcessOutputWatcher() = default;

void ProcessOutputWatcher::Start() {
  WatchProcessOutput();
}

void ProcessOutputWatcher::OnProcessOutputCanReadWithoutBlocking() {
  output_file_watcher_.reset();
  ReadFromFd(process_output_file_.GetPlatformFile());
}

void ProcessOutputWatcher::WatchProcessOutput() {
  output_file_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      process_output_file_.GetPlatformFile(),
      base::BindRepeating(
          &ProcessOutputWatcher::OnProcessOutputCanReadWithoutBlocking,
          base::Unretained(this)));
}

void ProcessOutputWatcher::ReadFromFd(int fd) {
  // We don't want to necessary read pipe until it is empty so we don't starve
  // other streams in case data is written faster than we read it. If there is
  // more than read_buffer_size_ bytes in pipe, it will be read in the next
  // iteration.
  DCHECK_GT(read_buffer_capacity_, read_buffer_size_);
  ssize_t bytes_read =
      HANDLE_EINTR(read(fd, &read_buffer_[read_buffer_size_],
                        read_buffer_capacity_ - read_buffer_size_));

  if (bytes_read > 0) {
    ReportOutput(PROCESS_OUTPUT_TYPE_OUT, bytes_read);
    return;
  }

  if (bytes_read < 0)
    DPLOG(WARNING) << "read from buffer failed";

  // If there is nothing on the output the watched process has exited (slave end
  // of pty is closed).
  on_read_callback_.Run(PROCESS_OUTPUT_TYPE_EXIT, "");
}

size_t ProcessOutputWatcher::OutputSizeWithoutIncompleteUTF8() {
  // Find the last non-trailing character byte. This byte should be used to
  // infer the last UTF8 character length.
  int last_lead_byte = read_buffer_size_ - 1;
  while (true) {
    // If the series of trailing bytes is too long, something's not right.
    // Report the whole output, without waiting for further character bytes.
    if (read_buffer_size_ - last_lead_byte > CBU8_MAX_LENGTH)
      return read_buffer_size_;

    // If there are trailing characters, there must be a leading one in the
    // buffer for a valid UTF8 character. Getting past the buffer begining
    // signals something's wrong, or the buffer is empty. In both cases return
    // the whole current buffer.
    if (last_lead_byte < 0)
      return read_buffer_size_;

    // Found the starting character byte; stop searching.
    if (!CBU8_IS_TRAIL(read_buffer_[last_lead_byte]))
      break;

    --last_lead_byte;
  }

  size_t last_length = UTF8SizeFromLeadingByte(read_buffer_[last_lead_byte]);

  // Note that if |last_length| == 0 or
  // |last_length| + |last_read_byte| < |read_buffer_size_|, the string is
  // invalid UTF8. In that case, send the whole read buffer to the observer
  // immediately, just as if there is no trailing incomplete UTF8 bytes.
  if (!last_length || last_length + last_lead_byte <= read_buffer_size_)
    return read_buffer_size_;

  return last_lead_byte;
}

void ProcessOutputWatcher::ReportOutput(ProcessOutputType type,
                                        size_t new_bytes_count) {
  read_buffer_size_ += new_bytes_count;
  size_t output_to_report = OutputSizeWithoutIncompleteUTF8();

  on_read_callback_.Run(type, std::string(read_buffer_, output_to_report));

  // Move the bytes that were left behind to the beginning of the buffer and
  // update the buffer size accordingly.
  if (output_to_report < read_buffer_size_) {
    for (size_t i = output_to_report; i < read_buffer_size_; ++i) {
      read_buffer_[i - output_to_report] = read_buffer_[i];
    }
  }
  read_buffer_size_ -= output_to_report;

  // Continue watching immediately if we don't have too many unacked outputs.
  if (++unacked_outputs_ <= kAckWindow) {
    WatchProcessOutput();
  }
}

void ProcessOutputWatcher::AckOutput() {
  if (--unacked_outputs_ == kAckWindow) {
    WatchProcessOutput();
  }
}

}  // namespace chromeos
